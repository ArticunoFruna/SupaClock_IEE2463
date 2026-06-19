"""
ECG Serial Monitor — Visualizador en tiempo real de señal ECG por puerto serial.

Lee datos del ESP32-C3 SuperMini vía USB-Serial-JTAG y los grafica usando
pyqtgraph (GPU-acelerado). Compatible con el firmware test_ecg_c3.

Formato esperado del firmware:
    ECG:<valor_adc>
    ECG:<valor_adc>,LEADS:OFF

Uso:
    python3 tools/ecg_serial_monitor.py                     # auto-detecta puerto
    python3 tools/ecg_serial_monitor.py --port /dev/ttyACM0 # puerto específico
    python3 tools/ecg_serial_monitor.py --baud 115200       # baudrate custom

Dependencias:
    pip install pyqtgraph PyQt6 pyserial numpy
"""

import sys
import os
import csv
import time
import threading
import argparse
from datetime import datetime
from collections import deque

import serial
import serial.tools.list_ports
import numpy as np

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QPushButton, QGroupBox, QFrame,
    QComboBox
)
from PyQt6.QtCore import pyqtSignal, QObject, Qt, QTimer
from PyQt6.QtGui import QFont

import pyqtgraph as pg

# ═══════════════════════════════════════════════════════════════════════
#                         Serial Worker
# ═══════════════════════════════════════════════════════════════════════
class SerialWorker(QObject):
    """Lee datos del puerto serial en un hilo separado."""
    ecg_received = pyqtSignal(int)        # valor ECG individual
    leads_off    = pyqtSignal()            # señal de electrodos desconectados
    status_changed = pyqtSignal(str)
    error_occurred = pyqtSignal(str)

    def __init__(self, port, baud):
        super().__init__()
        self.port = port
        self.baud = baud
        self.running = True
        self.ser = None

    def start_reading(self):
        """Ejecutar en un hilo daemon."""
        while self.running:
            try:
                self.status_changed.emit(f"Conectando a {self.port}…")
                self.ser = serial.Serial(self.port, self.baud, timeout=1.0)
                self.status_changed.emit(f"Conectado a {self.port} @ {self.baud}")

                while self.running and self.ser.is_open:
                    try:
                        raw = self.ser.readline()
                        if not raw:
                            continue

                        line = raw.decode('utf-8', errors='ignore').strip()

                        if line.startswith("ECG:"):
                            parts = line.split(",")
                            val_str = parts[0].split(":")[1]
                            try:
                                val = int(val_str)
                            except ValueError:
                                continue

                            if len(parts) > 1 and "LEADS:OFF" in parts[1]:
                                self.leads_off.emit()
                            else:
                                self.ecg_received.emit(val)

                    except (serial.SerialException, OSError):
                        break

                if self.ser and self.ser.is_open:
                    self.ser.close()

                self.status_changed.emit("Desconectado")

            except serial.SerialException as e:
                self.error_occurred.emit(f"Error serial: {e}")
                time.sleep(2.0)

            except Exception as e:
                self.error_occurred.emit(f"Error: {e}")
                time.sleep(2.0)

    def stop(self):
        self.running = False
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass


# ═══════════════════════════════════════════════════════════════════════
#                         Styles & Theme
# ═══════════════════════════════════════════════════════════════════════
DARK_STYLE = """
QMainWindow { background-color: #0d1117; }
QWidget { color: #e6edf3; font-family: 'Inter', 'Segoe UI', sans-serif; }
QGroupBox {
    border: 1px solid #30363d;
    border-radius: 8px;
    margin-top: 16px;
    padding-top: 20px;
    font-weight: 600;
    font-size: 13px;
    color: #8b949e;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
}
QLabel { font-size: 14px; }
QPushButton {
    border-radius: 6px;
    padding: 8px 16px;
    font-weight: 600;
    font-size: 13px;
}
QComboBox {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 6px 12px;
    color: #e6edf3;
    font-size: 13px;
}
QComboBox::drop-down {
    border: none;
}
QComboBox QAbstractItemView {
    background-color: #161b22;
    color: #e6edf3;
    selection-background-color: #30363d;
}
"""


# ═══════════════════════════════════════════════════════════════════════
#                         Main Window
# ═══════════════════════════════════════════════════════════════════════
class ECGSerialMonitor(QMainWindow):
    def __init__(self, port, baud):
        super().__init__()
        self.setWindowTitle("ECG Serial Monitor — SupaClock C3")
        self.resize(1100, 650)
        self.setStyleSheet(DARK_STYLE)

        self.port = port
        self.baud = baud

        # ── Recording state ──
        self.is_recording = False
        self.csv_file = None
        self.csv_writer = None

        # ── ECG buffer (3000 pts = 6s @ 500 Hz) ──
        self.N_ECG = 3000
        self.buf_ecg = np.zeros(self.N_ECG)

        # ── Stats ──
        self.sample_count = 0
        self.leads_off_flag = False
        self.last_value = 0
        self.samples_per_sec = 0
        self._sec_counter = 0

        self._build_ui()
        self._start_serial()

        # ── GUI refresh timer (30 FPS) ──
        self._timer = QTimer()
        self._timer.timeout.connect(self._refresh)
        self._timer.start(33)

        # ── Stats timer (1 Hz) ──
        self._stats_timer = QTimer()
        self._stats_timer.timeout.connect(self._update_stats)
        self._stats_timer.start(1000)

    # ─────────────────────────── UI ────────────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(12, 8, 12, 8)
        root.setSpacing(8)

        # ── Top bar: status + REC ──
        top = QHBoxLayout()

        self.lbl_status = QLabel("⬤ Desconectado")
        self.lbl_status.setFont(QFont("Inter", 15, QFont.Weight.Bold))
        self.lbl_status.setStyleSheet("color: #f85149;")

        self.btn_rec = QPushButton("⏺  REC")
        self.btn_rec.setFixedSize(130, 38)
        self.btn_rec.setStyleSheet(
            "background-color: #da3633; color: white; border: none;"
        )
        self.btn_rec.clicked.connect(self._toggle_rec)

        top.addWidget(self.lbl_status)
        top.addStretch()
        top.addWidget(self.btn_rec)
        root.addLayout(top)

        # ── Info cards ──
        cards_layout = QHBoxLayout()

        self.card_value = self._make_card("📊  Último Valor", "0", "#79c0ff")
        self.card_rate  = self._make_card("⚡  Muestras/s", "0 Hz", "#3fb950")
        self.card_total = self._make_card("📈  Total Muestras", "0", "#a371f7")
        self.card_leads = self._make_card("🔌  Electrodos", "OK", "#3fb950")

        cards_layout.addWidget(self.card_value[0])
        cards_layout.addWidget(self.card_rate[0])
        cards_layout.addWidget(self.card_total[0])
        cards_layout.addWidget(self.card_leads[0])
        root.addLayout(cards_layout)

        # ── ECG Plot ──
        pg.setConfigOptions(antialias=True)

        self.pw_ecg = pg.PlotWidget(title="Electrocardiograma (ECG) — Serial USB")
        self.pw_ecg.setBackground('#0d1117')
        self.pw_ecg.showGrid(x=True, y=True, alpha=0.2)
        self.pw_ecg.setLabel('left', 'ADC Raw')
        self.pw_ecg.setLabel('bottom', 'Muestras')
        self.c_ecg = self.pw_ecg.plot(
            pen=pg.mkPen('#3fb950', width=1.5), name="ECG"
        )

        root.addWidget(self.pw_ecg, stretch=1)

    def _make_card(self, title, default_val, accent_color):
        """Crea una tarjeta de info con estilo premium."""
        frame = QFrame()
        frame.setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d; "
            "border-radius: 10px; padding: 10px; }"
        )
        lay = QVBoxLayout(frame)
        t = QLabel(title)
        t.setFont(QFont("Inter", 11))
        t.setStyleSheet("color: #8b949e; border: none;")
        v = QLabel(default_val)
        v.setFont(QFont("Inter", 20, QFont.Weight.Bold))
        v.setStyleSheet(f"color: {accent_color}; border: none;")
        lay.addWidget(t)
        lay.addWidget(v)
        return frame, v

    # ─────────────────────────── Serial ───────────────────────────────
    def _start_serial(self):
        self._worker = SerialWorker(self.port, self.baud)
        self._worker.ecg_received.connect(self._on_ecg)
        self._worker.leads_off.connect(self._on_leads_off)
        self._worker.status_changed.connect(self._on_status)
        self._worker.error_occurred.connect(self._on_error)

        t = threading.Thread(target=self._worker.start_reading, daemon=True)
        t.start()

    def _on_status(self, text):
        self.lbl_status.setText(f"⬤ {text}")
        if "Conectado a" in text:
            self.lbl_status.setStyleSheet("color: #3fb950;")
        elif "Error" in text:
            self.lbl_status.setStyleSheet("color: #f85149;")
        else:
            self.lbl_status.setStyleSheet("color: #d29922;")

    def _on_error(self, text):
        self.lbl_status.setText(f"⬤ {text}")
        self.lbl_status.setStyleSheet("color: #f85149;")

    def _on_ecg(self, val):
        # Shift buffer e insertar nueva muestra
        self.buf_ecg[:-1] = self.buf_ecg[1:]
        self.buf_ecg[-1] = val
        self.sample_count += 1
        self._sec_counter += 1
        self.last_value = val
        self.leads_off_flag = False

        # CSV logging
        if self.is_recording and self.csv_writer:
            ts = int(time.time() * 1000)
            self.csv_writer.writerow([ts, val])

    def _on_leads_off(self):
        self.leads_off_flag = True

    # ─────────────────────── Plot refresh ─────────────────────────
    def _refresh(self):
        self.c_ecg.setData(self.buf_ecg)

    def _update_stats(self):
        self.card_value[1].setText(str(self.last_value))
        self.card_rate[1].setText(f"{self._sec_counter} Hz")
        self.card_total[1].setText(f"{self.sample_count:,}")

        if self.leads_off_flag:
            self.card_leads[1].setText("DESCONECTADOS")
            self.card_leads[1].setStyleSheet("color: #f85149; border: none;")
        else:
            self.card_leads[1].setText("OK")
            self.card_leads[1].setStyleSheet("color: #3fb950; border: none;")

        self._sec_counter = 0

    # ──────────────────────── Recording ───────────────────────────
    def _toggle_rec(self):
        if not self.is_recording:
            fname = datetime.now().strftime("ecg_serial_%Y%m%d_%H%M%S.csv")
            fpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), fname)
            try:
                self.csv_file = open(fpath, 'w', newline='')
                self.csv_writer = csv.writer(self.csv_file)
                self.csv_writer.writerow(['timestamp_ms', 'ecg_raw'])
                self.is_recording = True
                self.btn_rec.setText("⏹  STOP")
                self.btn_rec.setStyleSheet(
                    "background-color: #484f58; color: #e6edf3; border: none;"
                )
                print(f"🔴 Grabando → {fpath}")
            except Exception as e:
                print(f"Error abriendo CSV: {e}")
        else:
            self.is_recording = False
            if self.csv_file:
                self.csv_file.close()
                self.csv_file = None
                self.csv_writer = None
            self.btn_rec.setText("⏺  REC")
            self.btn_rec.setStyleSheet(
                "background-color: #da3633; color: white; border: none;"
            )
            print("⬜ Grabación detenida.")

    # ──────────────────────── Cleanup ─────────────────────────────
    def closeEvent(self, event):
        self._worker.stop()
        if self.is_recording and self.csv_file:
            self.csv_file.close()
        super().closeEvent(event)


# ═══════════════════════════════════════════════════════════════════════
#                         Auto-detect port
# ═══════════════════════════════════════════════════════════════════════
def find_esp32_port():
    """Intenta auto-detectar el puerto del ESP32-C3."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        # ESP32-C3 USB-JTAG aparece como "USB JTAG/serial debug unit"
        if "jtag" in desc or "esp" in desc or "303a" in hwid:
            print(f"🔍 Auto-detectado: {p.device} ({p.description})")
            return p.device

    # Fallback: intentar los puertos comunes
    for candidate in ["/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyUSB0"]:
        if os.path.exists(candidate):
            print(f"🔍 Usando puerto por defecto: {candidate}")
            return candidate

    return None


# ═══════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="ECG Serial Monitor — Visualiza ECG del ESP32-C3 por USB"
    )
    parser.add_argument("--port", "-p", type=str, default=None,
                        help="Puerto serial (ej: /dev/ttyACM0)")
    parser.add_argument("--baud", "-b", type=int, default=115200,
                        help="Baudrate (default: 115200)")
    args = parser.parse_args()

    port = args.port
    if port is None:
        port = find_esp32_port()
        if port is None:
            print("❌ No se encontró ningún puerto serial. Usa --port para especificarlo.")
            sys.exit(1)

    print(f"📡 ECG Serial Monitor")
    print(f"   Puerto: {port}")
    print(f"   Baud:   {args.baud}")
    print()

    app = QApplication(sys.argv)
    window = ECGSerialMonitor(port, args.baud)
    window.show()
    sys.exit(app.exec())
