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

    receiver = DataReceiver()
    thread = threading.Thread(target=receiver.receiver_loop, daemon=True)

    # conexiones con graph
    # conectar recepcion de mediciones
    receiver.data_recvd_temp.connect(gui.plot_temp.update_point)
    receiver.data_recvd_hum.connect(gui.plot_hum.update_point)
    receiver.data_recvd_pres.connect(gui.plot_pres.update_point)
    receiver.data_recvd_avg.connect(gui.plot_temp.change_avg)
    receiver.data_recvd_peak.connect(gui.plot_temp.addPico)

    # conexiones con conf_datos
    # conectar recepción checkboxes 
    gui.data_config.request_envio.connect(receiver.request_set_envio)
    grafs = [gui.plot_temp, gui.plot_hum, gui.plot_pres]
    receiver.toggle_envio.connect(lambda var, val: grafs[var].toggle_visibility(val)) #TODO: encontrar solucion para que esto sea seguro (hint: herencia?)
    receiver.toggle_envio.connect(gui.data_config.set_envio)

    # conectar recepción cambios en frecuencia de muestreo
    #gui.data_config.ui.comboBox_frec_muestreo.currentTextChanged.connect(receiver.request_set_frecuencia_muestreo)
    gui.data_config.request_frecuencia.connect(receiver.request_set_frecuencia_muestreo)
    receiver.frec_changed.connect(gui.data_config.set_frecuencia)

    # conectar con habilitar
    gui.data_config.request_habilitar.connect(receiver.request_set_habilitar)
    receiver.toggle_habilitar.connect(gui.data_config.set_habilitar)
    receiver.toggle_habilitar.connect(gui.plot_temp.set_habilitar)

    # conectar con umbral
    gui.data_config.request_umbral.connect(receiver.request_set_umbral)
    receiver.umbral_changed.connect(gui.data_config.set_umbral)

    # conexiones con conexion
    gui.conexion_config.conectar_toggled.connect(receiver.conectar)
    receiver.toggle_conectar.connect(gui.conexion_config.toggle_conectar)
    gui.conexion_config.ui.comboBox_puerto.currentTextChanged.connect(receiver.set_port)
    gui.conexion_config.ui.comboBox_baud_rate.currentTextChanged.connect(receiver.set_baud)

    gui.data_config.request_ventana.connect(receiver.request_set_ventana)
    receiver.ventana_changed.connect(gui.data_config.set_ventana)

    receiver.request_full_data_config.connect(gui.data_config.emit_full_data_config)
    gui.data_config.send_full_data_config.connect(receiver.get_full_data_config)

    window.statusBar().showMessage("Estado ESP32: Desconectado")
    receiver.status_recvd.connect(lambda s: window.statusBar().showMessage(f"Estado ESP32: {s}"))

    receiver.error_msg.connect(gui.error_popup)
    
    thread.start()
    window.show()
    app.exec()