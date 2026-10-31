import time
import random as rnd
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from gui import GUI

class DataReceiver(QObject):

    data_recvd_temp = pyqtSignal(float)
    data_recvd_hum = pyqtSignal(float)
    data_recvd_pres = pyqtSignal(float)
    data_recvd_avg = pyqtSignal(float)
    data_recvd_peak = pyqtSignal(float)

    toggle_conectar = pyqtSignal()
    umbral_changed = pyqtSignal(float)
    frec_changed = pyqtSignal(str)

    def __init__(self, gui: GUI):
        super().__init__()
        # TODO: quitar gui
        self.gui = gui

        self.ser = None
        self.conectado = False

    def receiver_loop(self):

        while True:

            t_measure = rnd.random() * 20
            h_measure = rnd.random() * 60 + 20
            p_measure = rnd.random() * 50 + 100

            self.data_recvd_temp.emit(t_measure)
            self.data_recvd_hum.emit(h_measure)
            self.data_recvd_pres.emit(p_measure)

            time.sleep(0.3)

    def send_uart(self):
        pass

    def recv_uart(self):
        pass

    # envía la solicitud a la esp
    @pyqtSlot(tuple)
    def request_set_envio(self, var: int, val: bool):
        """
        info (tuple): var, bool
        var = 0: temperatura;
        var = 1: humedad;
        var = 2: presion
        """
        # construir mensaje
        # enviar solicitud
        self.send_uart()

        # mientras no haya conexion real
        self.set_envio(var, val)

    # recibe la confirmacion de la esp
    def set_envio(self, var: int, val: bool):
        """
        var = 0: temperatura
        var = 1: humedad
        var = 2: presion
        """
        boxes = [self.gui.data_config.ui.checkBox_temp, 
                 self.gui.data_config.ui.checkBox_hum, 
                 self.gui.data_config.ui.checkBox_presion]

        graphs = [self.gui.plot_temp.ui.GraphSpace, 
                  self.gui.plot_hum,
                  self.gui.plot_pres,
                  self.gui.plot_temp.ui.label_seccion_media]
        
        boxes[var].blockSignals(True)
        boxes[var].setChecked(val)
        boxes[var].blockSignals(False)

        if val:
            if var == 0 and self.gui.plot_temp.isHidden():
                self.gui.plot_temp.show()
            graphs[var].show()
        else:
            graphs[var].hide()
            cond = self.gui.data_config.ui.checkBox_habilitar_picos.isChecked() or self.gui.data_config.ui.checkBox_habilitar_media.isChecked()
            if not cond:
                self.gui.plot_temp.hide()

    def request_set_habilitar(self, var: int):

        boxes = [self.gui.data_config.ui.checkBox_habilitar_media,
                self.gui.data_config.ui.checkBox_habilitar_picos]

        new_val = boxes[var].isChecked()
        boxes[var].blockSignals(True)
        boxes[var].setChecked(not new_val)
        boxes[var].blockSignals(False)

        self.set_habilitar(var, new_val)

    def set_habilitar(self, var: int, b: bool):

        boxes = [self.gui.data_config.ui.checkBox_habilitar_media,
                self.gui.data_config.ui.checkBox_habilitar_picos]

        items = [(self.gui.plot_temp.ui.label_seccion_media, self.gui.plot_temp.ui.label_media_2),
                 (self.gui.plot_temp.ui.label_seccion_picos, self.gui.plot_temp.ui.tableWidget_picos)]

        boxes[var].blockSignals(True)
        boxes[var].setChecked(b)
        boxes[var].blockSignals(False)

        print(f"cambiando {var} a {b}")

        if b:
            
            if self.gui.plot_temp.isHidden():
                self.gui.plot_temp.show()
            
            for it in items[var]:
                it.show()

        else:
            for it in items[var]:
                it.hide()

            # todo está escondido
            if not boxes[1-var].isChecked() and not self.gui.data_config.ui.checkBox_temp.isChecked():
                self.gui.plot_temp.hide()

    @pyqtSlot(str)
    def request_set_frecuencia_muestreo(self, v: str):
        #print("request a set_frecuencia desde receiver")
        self.send_uart()
        self.set_frecuencia_muestreo(v)

    def set_frecuencia_muestreo(self, v: str):
        #print("llamando a set_frecuencia desde receiver")
        self.frec_changed.emit(v)

    @pyqtSlot(float)
    def request_set_umbral(self, v: float):
        #print("request a set_umbral desde receiver")
        self.set_umbral(v)

    def set_umbral(self, v: float):
        #print("llamando a set_umbral desde reciever")
        self.umbral_changed.emit(v)

    @pyqtSlot()
    def conectar(self):
        self.toggle_conectar.emit()
