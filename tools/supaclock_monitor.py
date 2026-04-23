"""
SupaClock Monitor v2 — Unified Dashboard
Merges supaclock_monitor.py + live_viewer.py into a single premium app.

Features:
  • Real-time 2D waveforms (pyqtgraph — GPU-accelerated, much faster than matplotlib)
  • 3D orientation cube via OpenGL
  • Sensor telemetry panel (temperature, battery, steps)
  • CSV Data Logger with REC button
  • Dark-themed premium UI
"""

import sys
import os
import csv
import struct
import time
import math
import queue
import asyncio
import threading
from datetime import datetime

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QPushButton, QGroupBox, QGridLayout,
    QFrame, QSplitter
)
from PyQt6.QtCore import pyqtSignal, QObject, Qt, QTimer
from PyQt6.QtGui import QFont, QColor, QPalette

from bleak import BleakClient, BleakScanner

import pyqtgraph as pg
import pyqtgraph.opengl as gl
import numpy as np

# ═══════════════════════════════════════════════════════════════════════
#                         BLE UUIDs & Config
# ═══════════════════════════════════════════════════════════════════════
DEVICE_NAME    = "SupaClock_BLE"
IMU_CHR_UUID   = "0000FF01-0000-1000-8000-00805F9B34FB"
SENSOR_CHR_UUID = "0000FF02-0000-1000-8000-00805F9B34FB"
ECG_CHR_UUID   = "0000FF03-0000-1000-8000-00805F9B34FB"
CMD_CHR_UUID   = "0000FF04-0000-1000-8000-00805F9B34FB"

# ═══════════════════════════════════════════════════════════════════════
#                         BLE Worker (Async Thread)
# ═══════════════════════════════════════════════════════════════════════
class BleWorker(QObject):
    """Runs Bleak in a daemon thread, emits Qt signals on data arrival."""
    imu_received    = pyqtSignal(tuple)
    sensor_received = pyqtSignal(tuple)
    ecg_received    = pyqtSignal(tuple)
    status_changed  = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.running = True
        self.loop = None
        self.client = None

    def start_loop(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self._run_ble())

    def send_command(self, cmd_val: int):
        if self.client and self.client.is_connected and self.loop:
            asyncio.run_coroutine_threadsafe(
                self.client.write_gatt_char(CMD_CHR_UUID, bytes([cmd_val]), response=False),
                self.loop
            )

    async def _run_ble(self):
        while self.running:
            try:
                self.status_changed.emit("Buscando SupaClock_BLE…")
                device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=5.0)

                if device is None:
                    self.status_changed.emit("Dispositivo no encontrado. Reintentando…")
                    await asyncio.sleep(3.0)
                    continue

                self.status_changed.emit(f"Conectando a {device.address}…")
                async with BleakClient(device, timeout=10.0) as client:
                    self.client = client
                    self.status_changed.emit("Conectado ✓")

                    await client.start_notify(IMU_CHR_UUID, self._on_imu)
                    await client.start_notify(SENSOR_CHR_UUID, self._on_sensor)
                    await client.start_notify(ECG_CHR_UUID, self._on_ecg)

                    while client.is_connected and self.running:
                        await asyncio.sleep(1.0)
                        
                    self.client = None

                self.status_changed.emit("Desconectado")

            except Exception as e:
                self.client = None
                self.status_changed.emit(f"Error: {e}")
                await asyncio.sleep(3.0)

    def _on_imu(self, _sender, data):
        if len(data) == 12:
            self.imu_received.emit(struct.unpack('<hhhhhh', data))

    def _on_sensor(self, _sender, data):
        if len(data) == 11:
            self.sensor_received.emit(struct.unpack('<hHIHB', data))
            
    def _on_ecg(self, _sender, data):
        # 10 samples of 16-bit int = 20 bytes
        if len(data) == 20:
            self.ecg_received.emit(struct.unpack('<10h', data))

    def stop(self):
        self.running = False


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
"""


# ═══════════════════════════════════════════════════════════════════════
#                         Main Window
# ═══════════════════════════════════════════════════════════════════════
class SupaClockMonitor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SupaClock Monitor v2")
        self.resize(1400, 800)
        self.setStyleSheet(DARK_STYLE)

        # ── Recording state ──
        self.is_recording = False
        self.csv_file = None
        self.csv_writer = None

        # ── Latest slow-sensor data (for CSV sync) ──
        self.sensor_cache = {
            'temp_c': 0.0, 'steps_hw': 0, 'steps_sw': 0,
            'bat_mv': 0, 'bat_soc': 0
        }

        # ── Rolling data buffers (200 pts ≈ 4s @ 50 Hz) ──
        N = 200
        self.buf_ax = np.zeros(N); self.buf_ay = np.zeros(N); self.buf_az = np.zeros(N)
        self.buf_gx = np.zeros(N); self.buf_gy = np.zeros(N); self.buf_gz = np.zeros(N)
        self.latest_accel = (0, 0, 0)

        # ── ECG buffer (1500 pts = 3s @ 500 Hz) ──
        N_ECG = 1500
        self.buf_ecg = np.zeros(N_ECG)
        self.ecg_mode = False

        self._build_ui()
        self._start_ble()

        # ── GUI refresh timer (30 FPS) ──
        self._timer = QTimer()
        self._timer.timeout.connect(self._refresh)
        self._timer.start(33)

    # ─────────────────────────── UI ────────────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(12, 8, 12, 8)
        root.setSpacing(8)

        # ── Top bar: status + REC + ECG ──
        top = QHBoxLayout()

        self.lbl_status = QLabel("⬤ Desconectado")
        self.lbl_status.setFont(QFont("Inter", 15, QFont.Weight.Bold))
        self.lbl_status.setStyleSheet("color: #f85149;")

        self.btn_ecg = QPushButton("❤️ Iniciar ECG")
        self.btn_ecg.setFixedSize(140, 38)
        self.btn_ecg.setStyleSheet(
            "background-color: #3fb950; color: white; border: none;"
        )
        self.btn_ecg.clicked.connect(self._toggle_ecg)

        self.btn_rec = QPushButton("⏺  REC")
        self.btn_rec.setFixedSize(130, 38)
        self.btn_rec.setStyleSheet(
            "background-color: #da3633; color: white; border: none;"
        )
        self.btn_rec.clicked.connect(self._toggle_rec)

        top.addWidget(self.lbl_status)
        top.addStretch()
        top.addWidget(self.btn_ecg)
        top.addWidget(self.btn_rec)
        root.addLayout(top)

        # ── Sensor cards (temp, battery, steps) ──
        cards = QGroupBox("Telemetría de Sensores")
        cg = QGridLayout()

        self.card_temp  = self._make_card("🌡  Temperatura",  "--.- °C",  "#f0883e")
        self.card_bat   = self._make_card("🔋  Batería",      "-.-- V / --%", "#3fb950")
        self.card_steps = self._make_card("👟  Pasos",         "SW 0 | HW 0",  "#a371f7")

        cg.addWidget(self.card_temp[0],  0, 0)
        cg.addWidget(self.card_bat[0],   0, 1)
        cg.addWidget(self.card_steps[0], 0, 2)
        cards.setLayout(cg)
        root.addWidget(cards)

        # ── Main content: plots (left) + 3D (right) ──
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left: pyqtgraph 2D plots
        plot_widget = QWidget()
        self.plot_layout = QVBoxLayout(plot_widget)
        self.plot_layout.setContentsMargins(0, 0, 0, 0)

        pg.setConfigOptions(antialias=True)

        self.pw_accel = pg.PlotWidget(title="Acelerómetro (raw)")
        self.pw_accel.setBackground('#0d1117')
        self.pw_accel.showGrid(x=True, y=True, alpha=0.15)
        self.pw_accel.addLegend(offset=(10, 10))
        self.c_ax = self.pw_accel.plot(pen=pg.mkPen('#ff6e6e', width=1.5), name="Ax")
        self.c_ay = self.pw_accel.plot(pen=pg.mkPen('#7ee787', width=1.5), name="Ay")
        self.c_az = self.pw_accel.plot(pen=pg.mkPen('#79c0ff', width=1.5), name="Az")

        self.pw_gyro = pg.PlotWidget(title="Giroscopio (raw)")
        self.pw_gyro.setBackground('#0d1117')
        self.pw_gyro.showGrid(x=True, y=True, alpha=0.15)
        self.pw_gyro.addLegend(offset=(10, 10))
        self.c_gx = self.pw_gyro.plot(pen=pg.mkPen('#ff9bce', width=1.5), name="Gx")
        self.c_gy = self.pw_gyro.plot(pen=pg.mkPen('#d2a8ff', width=1.5), name="Gy")
        self.c_gz = self.pw_gyro.plot(pen=pg.mkPen('#a5d6ff', width=1.5), name="Gz")

        self.pw_ecg = pg.PlotWidget(title="Electrocardiograma (ECG)")
        self.pw_ecg.setBackground('#0d1117')
        self.pw_ecg.showGrid(x=True, y=True, alpha=0.2)
        self.pw_ecg.hide() # Oculto por defecto
        self.c_ecg = self.pw_ecg.plot(pen=pg.mkPen('#3fb950', width=2.0), name="ECG")

        self.plot_layout.addWidget(self.pw_accel)
        self.plot_layout.addWidget(self.pw_gyro)
        self.plot_layout.addWidget(self.pw_ecg)

        # Right: 3D orientation viewer
        self.view3d = gl.GLViewWidget()
        self.view3d.opts['distance'] = 40
        self.view3d.setMinimumWidth(300)

        grid = gl.GLGridItem()
        grid.setColor((60, 60, 60, 120))
        self.view3d.addItem(grid)

        axis = gl.GLAxisItem(glOptions='opaque')
        axis.setSize(x=10, y=10, z=10)
        self.view3d.addItem(axis)

        self.box3d = gl.GLBoxItem()
        self.box3d.setSize(x=6, y=6, z=2)
        self.box3d.translate(-3, -3, -1)
        self.view3d.addItem(self.box3d)

        splitter.addWidget(plot_widget)
        splitter.addWidget(self.view3d)
        splitter.setSizes([900, 400])
        root.addWidget(splitter, stretch=1)

    def _make_card(self, title, default_val, accent_color):
        """Create a styled sensor info card; returns (frame, value_label)."""
        frame = QFrame()
        frame.setStyleSheet(
            f"QFrame {{ background: #161b22; border: 1px solid #30363d; border-radius: 10px; padding: 10px; }}"
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

    # ─────────────────────────── BLE ───────────────────────────────
    def _start_ble(self):
        self._ble = BleWorker()
        self._ble.imu_received.connect(self._on_imu)
        self._ble.sensor_received.connect(self._on_sensor)
        self._ble.ecg_received.connect(self._on_ecg)
        self._ble.status_changed.connect(self._on_status)

        t = threading.Thread(target=self._ble.start_loop, daemon=True)
        t.start()

    def _on_status(self, text):
        self.lbl_status.setText(f"⬤ {text}")
        if "Conectado" in text:
            self.lbl_status.setStyleSheet("color: #3fb950;")
        elif "Error" in text or "no encontrado" in text:
            self.lbl_status.setStyleSheet("color: #f85149;")
        else:
            self.lbl_status.setStyleSheet("color: #d29922;")

    def _toggle_ecg(self):
        # Si estábamos grabando, lo detenemos para evitar inconsistencias de esquema CSV
        if self.is_recording:
            self._toggle_rec()
            
        if not self.ecg_mode:
            self.ecg_mode = True
            self.btn_ecg.setText("❤️ Detener ECG")
            self.btn_ecg.setStyleSheet("background-color: #da3633; color: white; border: none;")
            
            # Ocultar IMU, mostrar ECG
            self.pw_accel.hide()
            self.pw_gyro.hide()
            self.view3d.hide()
            self.pw_ecg.show()
            
            # Enviar comando de inicio
            self._ble.send_command(0x01)
        else:
            self.ecg_mode = False
            self.btn_ecg.setText("❤️ Iniciar ECG")
            self.btn_ecg.setStyleSheet("background-color: #3fb950; color: white; border: none;")
            
            # Ocultar ECG, mostrar IMU
            self.pw_ecg.hide()
            self.pw_accel.show()
            self.pw_gyro.show()
            self.view3d.show()
            
            # Enviar comando de parada
            self._ble.send_command(0x00)

    def _on_ecg(self, vals):
        # Shift buffer and insert 10 new samples
        n = len(vals)
        self.buf_ecg[:-n] = self.buf_ecg[n:]
        self.buf_ecg[-n:] = vals
        
        if self.is_recording and self.csv_writer and self.csv_mode == 'ecg':
            # Asignar timestamps (2ms por muestra hacia atrás)
            ts_end = int(time.time() * 1000)
            for i, val in enumerate(vals):
                ts = ts_end - (n - 1 - i) * 2
                self.csv_writer.writerow([ts, val])

    def _on_imu(self, vals):
        if self.ecg_mode: return # Ignorar IMU si llega alguno residual

        ax, ay, az, gx, gy, gz = vals

        # Shift buffers
        self.buf_ax[:-1] = self.buf_ax[1:]; self.buf_ax[-1] = ax
        self.buf_ay[:-1] = self.buf_ay[1:]; self.buf_ay[-1] = ay
        self.buf_az[:-1] = self.buf_az[1:]; self.buf_az[-1] = az
        self.buf_gx[:-1] = self.buf_gx[1:]; self.buf_gx[-1] = gx
        self.buf_gy[:-1] = self.buf_gy[1:]; self.buf_gy[-1] = gy
        self.buf_gz[:-1] = self.buf_gz[1:]; self.buf_gz[-1] = gz

        self.latest_accel = (ax, ay, az)

        # CSV logging at IMU rate
        if self.is_recording and self.csv_writer and self.csv_mode == 'imu':
            ts = int(time.time() * 1000)
            s = self.sensor_cache
            self.csv_writer.writerow([
                ts, ax, ay, az, gx, gy, gz,
                s['temp_c'], s['steps_hw'], s['steps_sw'],
                s['bat_mv'], s['bat_soc']
            ])

    def _on_sensor(self, vals):
        temp_x100, steps_hw, steps_sw, bat_mv, bat_soc = vals
        temp_c = temp_x100 / 100.0

        self.sensor_cache.update({
            'temp_c': temp_c, 'steps_hw': steps_hw,
            'steps_sw': steps_sw, 'bat_mv': bat_mv, 'bat_soc': bat_soc
        })

        self.card_temp[1].setText(f"{temp_c:.2f} °C")
        self.card_bat[1].setText(f"{bat_mv / 1000.0:.2f} V / {bat_soc}%")
        self.card_steps[1].setText(f"SW {steps_sw}  |  HW {steps_hw}")

    # ─────────────────────── Plot refresh ─────────────────────────
    def _refresh(self):
        if self.ecg_mode:
            self.c_ecg.setData(self.buf_ecg)
        else:
            # 2D waveforms
            self.c_ax.setData(self.buf_ax)
            self.c_ay.setData(self.buf_ay)
            self.c_az.setData(self.buf_az)
            self.c_gx.setData(self.buf_gx)
            self.c_gy.setData(self.buf_gy)
            self.c_gz.setData(self.buf_gz)

            # 3D orientation (pitch/roll from accel)
            ax, ay, az = self.latest_accel
            if ax != 0 or ay != 0 or az != 0:
                pitch = math.atan2(-ax, math.sqrt(ay * ay + az * az)) * 180.0 / math.pi
                roll  = math.atan2(ay, az) * 180.0 / math.pi

                self.box3d.resetTransform()
                self.box3d.translate(-3, -3, -1)
                self.box3d.rotate(pitch, 0, 1, 0)
                self.box3d.rotate(roll,  1, 0, 0)

    # ──────────────────────── Recording ───────────────────────────
    def _toggle_rec(self):
        if not self.is_recording:
            if self.ecg_mode:
                prefix = "supaclock_ecg_"
                headers = ['timestamp_ms', 'ecg_raw']
                self.csv_mode = 'ecg'
            else:
                prefix = "supaclock_imu_"
                headers = ['timestamp_ms', 'ax', 'ay', 'az', 'gx', 'gy', 'gz', 'temp_c', 'steps_hw', 'steps_sw', 'bat_mv', 'bat_soc']
                self.csv_mode = 'imu'

            fname = datetime.now().strftime(f"{prefix}%Y%m%d_%H%M%S.csv")
            fpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), fname)
            try:
                self.csv_file = open(fpath, 'w', newline='')
                self.csv_writer = csv.writer(self.csv_file)
                self.csv_writer.writerow(headers)
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
            self.csv_mode = None
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
        self._ble.stop()
        if self.is_recording and self.csv_file:
            self.csv_file.close()
        super().closeEvent(event)


# ═══════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SupaClockMonitor()
    window.show()
    sys.exit(app.exec())
