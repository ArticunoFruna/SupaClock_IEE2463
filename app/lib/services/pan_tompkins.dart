import 'dart:math' as math;

/// Pan-Tompkins QRS detector (1985), adapted Dart implementation.
///
/// Pipeline:
///   1. Bandpass (low-pass 5-15 Hz approx via cascaded LP+HP IIR)
///   2. Derivative (5-tap)
///   3. Squaring
///   4. Moving-window integration (window = 0.150 s)
///   5. Adaptive thresholding with refractory period (200 ms)
///
/// Designed for sample rates around 250-500 Hz (firmware streams 500 Hz ECG).
class PanTompkinsResult {
  final List<int> rPeakIndices;
  final List<double> rrIntervalsMs;
  final double bpmMean;
  final double? sdnnMs;
  final double? rmssdMs;
  final double qualityScore; // 0-1

  PanTompkinsResult({
    required this.rPeakIndices,
    required this.rrIntervalsMs,
    required this.bpmMean,
    this.sdnnMs,
    this.rmssdMs,
    this.qualityScore = 1.0,
  });
}

class PanTompkins {
  /// Run the full pipeline. [samples] is the raw ECG (any units).
  static PanTompkinsResult analyze(List<double> samples, {required double sampleRate}) {
    if (samples.length < (sampleRate * 2)) {
      return PanTompkinsResult(
          rPeakIndices: const [], rrIntervalsMs: const [], bpmMean: 0, qualityScore: 0);
    }

    final filtered = _bandpass(samples, sampleRate);
    final derivative = _derivative(filtered);
    final squared = derivative.map((v) => v * v).toList(growable: false);
    final integrated = _movingWindowIntegration(squared, (0.150 * sampleRate).round());

    final rawPeaks = _detectPeaks(integrated, sampleRate);
    final refined = _refineOnRaw(samples, rawPeaks, sampleRate);

    final rrMs = <double>[];
    for (var i = 1; i < refined.length; i++) {
      rrMs.add((refined[i] - refined[i - 1]) / sampleRate * 1000.0);
    }
    final rrFiltered = rrMs.where((rr) => rr >= 300 && rr <= 2000).toList();

    final hr = rrFiltered.isEmpty ? 0.0 : 60000.0 / _mean(rrFiltered);
    final sdnn = rrFiltered.length > 2 ? _stdev(rrFiltered) : null;
    double? rmssd;
    if (rrFiltered.length > 2) {
      final diffs = <double>[];
      for (var i = 1; i < rrFiltered.length; i++) {
        diffs.add(rrFiltered[i] - rrFiltered[i - 1]);
      }
      rmssd = math.sqrt(diffs.map((d) => d * d).fold(0.0, (a, b) => a + b) / diffs.length);
    }

    final quality = _qualityScore(rrFiltered, refined.length, samples.length, sampleRate);

    return PanTompkinsResult(
      rPeakIndices: refined,
      rrIntervalsMs: rrMs,
      bpmMean: hr,
      sdnnMs: sdnn,
      rmssdMs: rmssd,
      qualityScore: quality,
    );
  }

  // ───────────────────────────── filters ────────────────────────────

  /// Lightweight bandpass approximation: LP (cutoff ~15 Hz) followed by HP (~5 Hz).
  /// Implemented as 1st-order IIR; not as sharp as the original integer filters
  /// but adequate for QRS detection.
  static List<double> _bandpass(List<double> x, double fs) {
    final lp = _lowpass(x, fs, 15);
    return _highpass(lp, fs, 5);
  }

  static List<double> _lowpass(List<double> x, double fs, double fc) {
    final rc = 1.0 / (2 * math.pi * fc);
    final dt = 1.0 / fs;
    final a = dt / (rc + dt);
    final out = List<double>.filled(x.length, 0);
    out[0] = x[0];
    for (var i = 1; i < x.length; i++) {
      out[i] = out[i - 1] + a * (x[i] - out[i - 1]);
    }
    return out;
  }

  static List<double> _highpass(List<double> x, double fs, double fc) {
    final rc = 1.0 / (2 * math.pi * fc);
    final dt = 1.0 / fs;
    final a = rc / (rc + dt);
    final out = List<double>.filled(x.length, 0);
    out[0] = x[0];
    for (var i = 1; i < x.length; i++) {
      out[i] = a * (out[i - 1] + x[i] - x[i - 1]);
    }
    return out;
  }

  static List<double> _derivative(List<double> x) {
    final out = List<double>.filled(x.length, 0);
    for (var i = 2; i < x.length - 2; i++) {
      out[i] = (-x[i - 2] - 2 * x[i - 1] + 2 * x[i + 1] + x[i + 2]) / 8.0;
    }
    return out;
  }

  static List<double> _movingWindowIntegration(List<double> x, int window) {
    final w = math.max(1, window);
    final out = List<double>.filled(x.length, 0);
    var sum = 0.0;
    for (var i = 0; i < x.length; i++) {
      sum += x[i];
      if (i >= w) sum -= x[i - w];
      out[i] = sum / w;
    }
    return out;
  }

  // ─────────────────────────── peak detection ───────────────────────

  static List<int> _detectPeaks(List<double> integrated, double fs) {
    final refractory = (0.200 * fs).round(); // 200 ms

    // Initial estimates
    var spki = 0.0;
    var npki = 0.0;
    var thresholdI1 = 0.0;
    var thresholdI2 = 0.0;

    // Bootstrap with the first 2 seconds
    final initLen = math.min(integrated.length, (2 * fs).round());
    var maxV = 0.0;
    var meanV = 0.0;
    for (var i = 0; i < initLen; i++) {
      meanV += integrated[i];
      if (integrated[i] > maxV) maxV = integrated[i];
    }
    meanV /= initLen.toDouble();
    spki = maxV * 0.25;
    npki = meanV * 0.5;
    thresholdI1 = npki + 0.25 * (spki - npki);
    thresholdI2 = thresholdI1 / 2.0;

    final peaks = <int>[];
    var lastPeak = -refractory;

    for (var i = 1; i < integrated.length - 1; i++) {
      final v = integrated[i];
      final isLocalMax = v > integrated[i - 1] && v >= integrated[i + 1];
      if (!isLocalMax) continue;

      if (v > thresholdI1 && (i - lastPeak) > refractory) {
        peaks.add(i);
        lastPeak = i;
        spki = 0.125 * v + 0.875 * spki;
      } else if (v > thresholdI2 &&
          peaks.isNotEmpty &&
          (i - peaks.last) > (1.5 * _meanRr(peaks, fs))) {
        // search-back
        peaks.add(i);
        lastPeak = i;
        spki = 0.25 * v + 0.75 * spki;
      } else {
        npki = 0.125 * v + 0.875 * npki;
      }

      thresholdI1 = npki + 0.25 * (spki - npki);
      thresholdI2 = thresholdI1 / 2.0;
    }

    // Translate integration-window indices to QRS apex by stepping back ~window/2
    final shift = (0.075 * fs).round();
    return peaks.map((p) => math.max(0, p - shift)).toList();
  }

  static double _meanRr(List<int> peaks, double fs) {
    if (peaks.length < 2) return fs.toDouble(); // 1 s default
    final rr = <double>[];
    for (var i = 1; i < peaks.length; i++) {
      rr.add((peaks[i] - peaks[i - 1]).toDouble());
    }
    return _mean(rr);
  }

  /// Within ±30 ms of each candidate, snap to the actual maximum on the
  /// raw waveform (gives a cleaner "R apex" location).
  static List<int> _refineOnRaw(List<double> raw, List<int> candidates, double fs) {
    final w = (0.030 * fs).round();
    final out = <int>[];
    for (final c in candidates) {
      final lo = math.max(0, c - w);
      final hi = math.min(raw.length - 1, c + w);
      var bestI = c;
      var bestV = raw[c];
      for (var i = lo; i <= hi; i++) {
        if (raw[i] > bestV) {
          bestV = raw[i];
          bestI = i;
        }
      }
      out.add(bestI);
    }
    return out;
  }

  // ─────────────────────────── quality score ────────────────────────
  static double _qualityScore(
      List<double> rrFiltered, int peakCount, int sampleCount, double fs) {
    if (peakCount < 3) return 0.0;
    final durSec = sampleCount / fs;
    final expectedPeaks = (durSec / 1.0).clamp(1.0, 999.0); // ~60 bpm baseline
    final ratio = (peakCount / expectedPeaks).clamp(0.0, 2.0);
    // good = 0.6..1.4 ratio. Outside that, penalize.
    final ratioScore = (1.0 - (ratio - 1.0).abs()).clamp(0.0, 1.0);
    final cv = rrFiltered.length > 2
        ? (_stdev(rrFiltered) / _mean(rrFiltered)).clamp(0.0, 1.0)
        : 0.5;
    final cvScore = (1.0 - cv).clamp(0.0, 1.0);
    return (0.6 * ratioScore + 0.4 * cvScore).clamp(0.0, 1.0);
  }

  // ─────────────────────────── helpers ──────────────────────────────
  static double _mean(List<double> xs) =>
      xs.isEmpty ? 0 : xs.reduce((a, b) => a + b) / xs.length;

  static double _stdev(List<double> xs) {
    if (xs.length < 2) return 0;
    final m = _mean(xs);
    final v = xs.map((x) => (x - m) * (x - m)).reduce((a, b) => a + b) / (xs.length - 1);
    return math.sqrt(v);
  }
}
