"""
Captura raw red/ir del MAX30102 (firmware env:test_spo2) y genera la
comparativa "antes vs después del bandpass" para la presentación.

Uso:
    python supaclock_raw_ppg.py [duracion_segundos]

Defaults: 30 s. Genera:
    raw_ppg_<timestamp>.csv     (timestamp_ms, red, ir)
    raw_ppg_<timestamp>.png     (4 paneles: ir crudo, ir bandpass,
                                 red crudo, red bandpass)
"""
import sys
import csv
import time
import struct
import asyncio
from datetime import datetime

import numpy as np
from scipy.signal import butter, sosfilt
import matplotlib.pyplot as plt

from bleak import BleakClient, BleakScanner

DEVICE_NAME  = "SupaClock_BLE"
ECG_CHR_UUID = "0000FF03-0000-1000-8000-00805F9B34FB"

FS = 25.0  # Hz efectivos del MAX30102 (sample_avg=4 sobre 100 sps)


def design_bandpass():
    """Mismo filtro que el firmware: cascada HPF 0.5 Hz + LPF 4 Hz, orden 2."""
    sos = butter(N=2, Wn=[0.5, 4.0], btype="bandpass", fs=FS, output="sos")
    return sos


async def capture(duration_s: int):
    print(f"Buscando {DEVICE_NAME}…")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if device is None:
        print("Dispositivo no encontrado.")
        return None, None

    samples = []  # (ts_ms, red, ir)
    t0 = None

    def on_notify(_sender, data: bytearray):
        nonlocal t0
        if t0 is None:
            t0 = time.time()
        ts_ms = int((time.time() - t0) * 1000)
        # data viene en chunks de N×8 bytes: [red_u32_le, ir_u32_le]
        n = len(data) // 8
        for k in range(n):
            red, ir = struct.unpack_from("<II", data, k * 8)
            samples.append((ts_ms, red, ir))

    print(f"Conectando a {device.address}…")
    async with BleakClient(device, timeout=15.0) as client:
        print("Conectado. Iniciando captura.")
        await client.start_notify(ECG_CHR_UUID, on_notify)
        await asyncio.sleep(duration_s)
        await client.stop_notify(ECG_CHR_UUID)

    print(f"Recibidas {len(samples)} muestras en {duration_s} s "
          f"(esperadas ~{int(FS * duration_s)})")

    ts_str = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = f"raw_ppg_{ts_str}.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["timestamp_ms", "red", "ir"])
        w.writerows(samples)
    print(f"CSV escrito: {csv_path}")
    return samples, csv_path


def plot_compare(samples, csv_path):
    if not samples:
        return
    arr = np.array(samples, dtype=np.int64)
    t = arr[:, 0] / 1000.0
    red = arr[:, 1].astype(float)
    ir  = arr[:, 2].astype(float)

    sos = design_bandpass()

    # DC removal (igual al firmware: EMA α = 0.05)
    def dc_remove(x, alpha=0.05):
        dc = np.zeros_like(x)
        dc[0] = x[0]
        for i in range(1, len(x)):
            dc[i] = dc[i - 1] + alpha * (x[i] - dc[i - 1])
        return x - dc

    ir_ac  = dc_remove(ir)
    red_ac = dc_remove(red)
    ir_bp  = sosfilt(sos, ir_ac)
    red_bp = sosfilt(sos, red_ac)

    fig, axes = plt.subplots(4, 1, figsize=(10, 9), sharex=True)
    axes[0].plot(t, ir,    color="#79c0ff");   axes[0].set_title("IR — crudo")
    axes[1].plot(t, ir_bp, color="#3fb950");   axes[1].set_title("IR — bandpass 0.5–4 Hz")
    axes[2].plot(t, red,   color="#ff6e6e");   axes[2].set_title("Red — crudo")
    axes[3].plot(t, red_bp,color="#3fb950");   axes[3].set_title("Red — bandpass 0.5–4 Hz")
    for ax in axes:
        ax.grid(True, alpha=0.3)
        ax.set_ylabel("ADC counts")
    axes[-1].set_xlabel("Tiempo [s]")
    fig.suptitle("MAX30102 — Comparativa antes/después del bandpass IIR",
                 fontweight="bold")
    fig.tight_layout()

    png_path = csv_path.replace(".csv", ".png")
    fig.savefig(png_path, dpi=130)
    print(f"PNG escrito: {png_path}")


async def main():
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    samples, csv_path = await capture(duration)
    if samples:
        plot_compare(samples, csv_path)


if __name__ == "__main__":
    asyncio.run(main())
