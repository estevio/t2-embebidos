import sys
import time
import threading
import random as rnd
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from PyQt6.QtWidgets import QApplication, QWidget, QMainWindow
from main_ui.main_ui import Ui_MainWindow
from graph import *
from conexion import AppConexion
from conf_datos import AppConfDatos

class DataReceiver(QObject):

    data_recvd_temp = pyqtSignal(float)
    data_recvd_hum = pyqtSignal(float)
    def __init__(self):
        super().__init__()

    def receiver_loop(self):

        while True:

            t_measure = rnd.random() * 20
            h_measure = rnd.random() * 60 + 20

            self.data_recvd_temp.emit(t_measure)
            self.data_recvd_hum.emit(h_measure)

            time.sleep(0.1)
            

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = QMainWindow()
    gui = Ui_MainWindow()
    gui.setupUi(window)

    # agregar configuracion de conexion
    conexion_config_ui = AppConexion()
    gui.conf_layout.addWidget(conexion_config_ui)

    # agregar configuracion adquisicion de datos
    data_config_ui = AppConfDatos()
    gui.conf_layout.addWidget(data_config_ui)

    # agregar gráficos
    plot_temp = AppGrafico("Temperatura")
    plot_hum = AppGrafico("Humedad Relativa")

    gui.graf_layout.addWidget(plot_temp)
    gui.graf_layout.addWidget(plot_hum)

    receiver = DataReceiver()
    thread = threading.Thread(target=receiver.receiver_loop, daemon=True)

    receiver.data_recvd_temp.connect(plot_temp.graf.add_point)
    receiver.data_recvd_hum.connect(plot_hum.graf.add_point)

    thread.start()
    window.show()
    app.exec()