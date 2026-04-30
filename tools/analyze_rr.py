import pandas as pd
import numpy as np
import sys
sys.path.append("/home/articunot/Documents/PlatformIO/Projects/SupaClock/scripts")
from test_pt import pan_tompkins

csv_files = [
    "/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/supaclock_ecg_20260430_140907.csv",
    "/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/supaclock_ecg_20260430_140819.csv"
]

for f in csv_files:
    df = pd.read_csv(f)
    raw_ecg = df['ecg_raw'].tolist()
    results = pan_tompkins(raw_ecg, fs=500.0)
    rr = results['rr_intervals']
    print(f"\n--- Analysis for {f.split('/')[-1]} ---")
    print(f"Total peaks: {len(results['r_peaks'])}")
    print(f"RR intervals (ms): {rr}")
    if len(rr) > 0:
        print(f"Mean RR: {np.mean(rr):.1f} ms, Min RR: {np.min(rr):.1f} ms, Max RR: {np.max(rr):.1f} ms")
        median_rr = np.median(rr)
        missed = [x for x in rr if x > 1.5 * median_rr]
        extra = [x for x in rr if x < 0.5 * median_rr]
        print(f"Likely missed peaks (RR > 1.5x median): {len(missed)} intervals -> {missed}")
        print(f"Likely false positives (RR < 0.5x median): {len(extra)} intervals -> {extra}")

