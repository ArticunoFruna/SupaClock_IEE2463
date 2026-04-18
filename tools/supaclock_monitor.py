import sys
import os
import csv
import struct
import time
import asyncio
import threading
from datetime import datetime

from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QPushButton, QGroupBox, QGridLayout)
from PyQt6.QtCore import pyqtSignal, QObject, Qt, QTimer
from PyQt6.QtGui import QFont, QColor
from bleak import BleakClient, BleakScanner

import matplotlib
matplotlib.use('QtAgg')
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure
import numpy as np

IMU_CHR_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"
SENSOR_CHR_UUID = "0000FF02-0000-1000-8000-00805F9B34FB"

class BleWorker(QObject):
    imu_received = pyqtSignal(tuple)
    sensor_received = pyqtSignal(tuple)
    status_changed = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.client = None
        self.loop = None
        self.running = True

    def start_loop(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self.run_ble())

    async def run_ble(self):
        while self.running:
            try:
                self.status_changed.emit("Buscando SupaClock_BLE...")
                devices = await BleakScanner.discover(timeout=5.0)
                target = None
                for d in devices:
                    if d.name == "SupaClock_BLE" or d.name == "SupaClock":
                        target = d
                        break

                if target:
                    self.status_changed.emit(f"Conectando a {target.address}...")
                    async with BleakClient(target.address) as client:
                        self.client = client
                        self.status_changed.emit("Conectado")
                        
                        await client.start_notify(IMU_CHR_UUID, self.imu_handler)
                        await client.start_notify(SENSOR_CHR_UUID, self.sensor_handler)

                        while client.is_connected and self.running:
                            await asyncio.sleep(1.0)
                            
                        self.client = None
                else:
                    await asyncio.sleep(2.0)
            except Exception as e:
                self.status_changed.emit(f"Error: {e}")
                await asyncio.sleep(2.0)

    def imu_handler(self, sender, data):
        if len(data) == 12:
            vals = struct.unpack('<hhhhhh', data) # ax, ay, az, gx, gy, gz
            self.imu_received.emit(vals)

    def sensor_handler(self, sender, data):
        if len(data) == 11:
            vals = struct.unpack('<hHIHB', data) # temp, steps_hw, steps_sw, bat_mv, bat_soc
            self.sensor_received.emit(vals)

    def stop(self):
        self.running = False


class MplCanvas(FigureCanvasQTAgg):
    def __init__(self, parent=None, width=5, height=4, dpi=100, title=""):
        fig = Figure(figsize=(width, height), dpi=dpi)
        fig.patch.set_facecolor('#1e1e1e')
        self.axes = fig.add_subplot(111)
        self.axes.set_facecolor('#1e1e1e')
        self.axes.tick_params(colors='white')
        self.axes.set_title(title, color='white')
        super().__init__(fig)
        self.setStyleSheet("background-color: transparent;")


class SupaClockMonitor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SupaClock Monitor & Data Logger")
        self.resize(1000, 700)
        self.setStyleSheet("background-color: #121212; color: #ffffff;")

        # Data states
        self.is_recording = False
        self.csv_file = None
        self.csv_writer = None
        self.latest_sensor_data = {
            'temp_c': 0.0,
            'steps_hw': 0,
            'steps_sw': 0,
            'bat_mv': 0,
            'bat_soc': 0.0
        }

        # Plot data (rolling windows of 150 samples ~ 3s at 50Hz)
        self.plot_size = 150
        self.accel_x = np.zeros(self.plot_size)
        self.accel_y = np.zeros(self.plot_size)
        self.accel_z = np.zeros(self.plot_size)
        self.gyro_x = np.zeros(self.plot_size)
        self.gyro_y = np.zeros(self.plot_size)
        self.gyro_z = np.zeros(self.plot_size)

        self.setup_ui()
        
        # Start BLE Thread
        self.ble_worker = BleWorker()
        self.ble_worker.imu_received.connect(self.on_imu_data)
        self.ble_worker.sensor_received.connect(self.on_sensor_data)
        self.ble_worker.status_changed.connect(self.update_status)
        
        self.ble_thread = threading.Thread(target=self.ble_worker.start_loop, daemon=True)
        self.ble_thread.start()

        # Update plots via timer (30 fps)
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_plots)
        self.timer.start(33)

    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Header controls
        header_layout = QHBoxLayout()
        
        self.status_label = QLabel("Estado: Desconectado")
        self.status_label.setFont(QFont("Arial", 14))
        self.status_label.setStyleSheet("color: #ff5555;")
        
        self.rec_btn = QPushButton("⏺ REC")
        self.rec_btn.setFixedSize(120, 40)
        self.rec_btn.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        self.rec_btn.setStyleSheet("background-color: #d32f2f; color: white; border-radius: 5px;")
        self.rec_btn.clicked.connect(self.toggle_recording)

        header_layout.addWidget(self.status_label)
        header_layout.addStretch()
        header_layout.addWidget(self.rec_btn)
        main_layout.addLayout(header_layout)

        # Labels for slow sensors
        sensors_group = QGroupBox("Sensores (1 Hz)")
        sensors_group.setStyleSheet("QGroupBox { border: 1px solid #444; margin-top: 1ex; font-weight: bold; font-size: 14px; }")
        sensors_layout = QGridLayout()
        
        large_font = QFont("Arial", 18)
        self.temp_label = QLabel("Temp: -- °C")
        self.temp_label.setFont(large_font)
        
        self.bat_label = QLabel("Batería: -- V / -- %")
        self.bat_label.setFont(large_font)
        
        self.steps_label = QLabel("Pasos: HW 0 | SW 0")
        self.steps_label.setFont(large_font)

        sensors_layout.addWidget(self.temp_label, 0, 0)
        sensors_layout.addWidget(self.bat_label, 0, 1)
        sensors_layout.addWidget(self.steps_label, 0, 2)
        sensors_group.setLayout(sensors_layout)
        main_layout.addWidget(sensors_group)

        # Plots for IMU
        plots_layout = QHBoxLayout()

        self.canvas_accel = MplCanvas(self, width=5, height=4, dpi=100, title="Acelerómetro (raw)")
        self.line_ax, = self.canvas_accel.axes.plot(self.accel_x, 'r', label='Ax')
        self.line_ay, = self.canvas_accel.axes.plot(self.accel_y, 'g', label='Ay')
        self.line_az, = self.canvas_accel.axes.plot(self.accel_z, 'b', label='Az')
        self.canvas_accel.axes.legend(loc='upper right', framealpha=0.5)
        self.canvas_accel.axes.set_ylim(-32768, 32767)

        self.canvas_gyro = MplCanvas(self, width=5, height=4, dpi=100, title="Giroscopio (raw)")
        self.line_gx, = self.canvas_gyro.axes.plot(self.gyro_x, 'r', label='Gx')
        self.line_gy, = self.canvas_gyro.axes.plot(self.gyro_y, 'g', label='Gy')
        self.line_gz, = self.canvas_gyro.axes.plot(self.gyro_z, 'b', label='Gz')
        self.canvas_gyro.axes.legend(loc='upper right', framealpha=0.5)
        self.canvas_gyro.axes.set_ylim(-32768, 32767)

        plots_layout.addWidget(self.canvas_accel)
        plots_layout.addWidget(self.canvas_gyro)
        main_layout.addLayout(plots_layout)

    def toggle_recording(self):
        if not self.is_recording:
            # Empezar a grabar
            filename = datetime.now().strftime("supaclock_%Y%m%d_%H%M%S.csv")
            filepath = os.path.join(os.getcwd(), filename)
            try:
                self.csv_file = open(filepath, 'w', newline='')
                self.csv_writer = csv.writer(self.csv_file)
                self.csv_writer.writerow(['timestamp_ms', 'ax', 'ay', 'az', 'gx', 'gy', 'gz', 
                                          'temp_c', 'steps_hw', 'steps_sw', 'bat_mv', 'bat_soc'])
                self.is_recording = True
                self.rec_btn.setText("⏹ STOP")
                self.rec_btn.setStyleSheet("background-color: #555555; color: white; border-radius: 5px;")
                print(f"Data logging iniciado: {filepath}")
            except Exception as e:
                print(f"Error abriendo CSV: {e}")
        else:
            # Parar de grabar
            self.is_recording = False
            if self.csv_file:
                self.csv_file.close()
                self.csv_file = None
                self.csv_writer = None
            self.rec_btn.setText("⏺ REC")
            self.rec_btn.setStyleSheet("background-color: #d32f2f; color: white; border-radius: 5px;")
            print("Data logging detenido.")

    def update_status(self, text):
        self.status_label.setText(f"Estado: {text}")
        if "Conectado" in text:
            self.status_label.setStyleSheet("color: #55ff55;")
        else:
            self.status_label.setStyleSheet("color: #ff5555;")

    def on_sensor_data(self, vals):
        temp_x100, steps_hw, steps_sw, bat_mv, bat_soc = vals
        
        self.latest_sensor_data['temp_c'] = temp_x100 / 100.0
        self.latest_sensor_data['steps_hw'] = steps_hw
        self.latest_sensor_data['steps_sw'] = steps_sw
        self.latest_sensor_data['bat_mv'] = bat_mv
        self.latest_sensor_data['bat_soc'] = bat_soc

        self.temp_label.setText(f"Temp: {self.latest_sensor_data['temp_c']:.2f} °C")
        self.bat_label.setText(f"Batería: {bat_mv/1000.0:.2f} V / {bat_soc} %")
        self.steps_label.setText(f"Pasos: HW {steps_hw} | SW {steps_sw}")

    def on_imu_data(self, vals):
        ax, ay, az, gx, gy, gz = vals
        
        # Shift buffers
        self.accel_x = np.roll(self.accel_x, -1)
        self.accel_y = np.roll(self.accel_y, -1)
        self.accel_z = np.roll(self.accel_z, -1)
        self.gyro_x = np.roll(self.gyro_x, -1)
        self.gyro_y = np.roll(self.gyro_y, -1)
        self.gyro_z = np.roll(self.gyro_z, -1)
        
        # New values
        self.accel_x[-1] = ax
        self.accel_y[-1] = ay
        self.accel_z[-1] = az
        self.gyro_x[-1] = gx
        self.gyro_y[-1] = gy
        self.gyro_z[-1] = gz

        # Data logging taking sync from IMU rate (approx 50Hz)
        if self.is_recording and self.csv_writer:
            ts_ms = int(time.time() * 1000)
            self.csv_writer.writerow([
                ts_ms, ax, ay, az, gx, gy, gz,
                self.latest_sensor_data['temp_c'],
                self.latest_sensor_data['steps_hw'],
                self.latest_sensor_data['steps_sw'],
                self.latest_sensor_data['bat_mv'],
                self.latest_sensor_data['bat_soc']
            ])

    def update_plots(self):
        self.line_ax.set_ydata(self.accel_x)
        self.line_ay.set_ydata(self.accel_y)
        self.line_az.set_ydata(self.accel_z)
        self.canvas_accel.draw_idle()

        self.line_gx.set_ydata(self.gyro_x)
        self.line_gy.set_ydata(self.gyro_y)
        self.line_gz.set_ydata(self.gyro_z)
        self.canvas_gyro.draw_idle()

    def closeEvent(self, event):
        self.ble_worker.stop()
        if self.is_recording and self.csv_file:
            self.csv_file.close()
        super().closeEvent(event)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SupaClockMonitor()
    window.show()
    sys.exit(app.exec())
