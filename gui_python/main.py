import sys
import threading
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from PyQt6.QtWidgets import QApplication, QWidget, QMainWindow
from main_ui.main_ui import Ui_MainWindow
from graph import *
from receiver import DataReceiver
from gui import GUI


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = QMainWindow()
    gui = GUI(window)

    receiver = DataReceiver(gui)
    thread = threading.Thread(target=receiver.receiver_loop, daemon=True)

    # conectar recepcion de mediciones
    receiver.data_recvd_temp.connect(gui.plot_temp.graf.add_point)
    receiver.data_recvd_hum.connect(gui.plot_hum.graf.add_point)
    receiver.data_recvd_pres.connect(gui.plot_pres.graf.add_point)

    # conectar recepción checkboxes
    gui.data_config.request_set_envio.connect(receiver.request_set_envio)

    # conectar recepción cambios en frecuencia de muestreo
    gui.data_config.ui.comboBox_frec_muestreo.currentTextChanged.connect(receiver.request_set_frecuencia_muestreo)

    thread.start()
    window.show()
    app.exec()