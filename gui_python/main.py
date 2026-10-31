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

    # conexiones con graph
    # conectar recepcion de mediciones
    receiver.data_recvd_temp.connect(gui.plot_temp.graf.add_point)
    receiver.data_recvd_hum.connect(gui.plot_hum.graf.add_point)
    receiver.data_recvd_pres.connect(gui.plot_pres.graf.add_point)
    receiver.data_recvd_avg.connect(gui.plot_temp.change_avg)
    receiver.data_recvd_peak.connect(gui.plot_temp.addPico)

    # conexiones con conf_datos
    # conectar recepción checkboxes 
    gui.data_config.request_envio.connect(receiver.request_set_envio)
    grafs = [gui.plot_temp, gui.plot_hum, gui.plot_pres]
    receiver.toggle_envio.connect(lambda var, val: grafs[var].toggle_visibility(val)) #TODO: encontrar solucion para que esto sea seguro (hint: herencia?)
    receiver.toggle_envio.connect(gui.data_config.set_envio)
    # conectar recepción cambios en frecuencia de muestreo
    gui.data_config.ui.comboBox_frec_muestreo.currentTextChanged.connect(receiver.request_set_frecuencia_muestreo)
    receiver.frec_changed.connect(gui.data_config.set_frecuencia)
    # conectar con habilitar
    gui.data_config.ui.checkBox_habilitar_media.toggled.connect(lambda: receiver.request_set_habilitar(0))
    gui.data_config.ui.checkBox_habilitar_picos.toggled.connect(lambda: receiver.request_set_habilitar(1))
    # conectar con umbral
    gui.data_config.request_umbral.connect(receiver.request_set_umbral)
    receiver.umbral_changed.connect(gui.data_config.set_umbral)
    # conectar con frecuencia

    # conexiones con conexion
    gui.conexion_config.conectar_toggled.connect(receiver.conectar)
    receiver.toggle_conectar.connect(gui.conexion_config.toggle_conectar)
    
    thread.start()
    window.show()
    app.exec()