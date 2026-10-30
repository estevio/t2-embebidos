import time
import random as rnd
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from gui import GUI

class DataReceiver(QObject):

    data_recvd_temp = pyqtSignal(float)
    data_recvd_hum = pyqtSignal(float)
    data_recvd_pres = pyqtSignal(float)

    def __init__(self, gui: GUI):
        super().__init__()
        self.gui = gui
        self.last_frec_index = gui.data_config.ui.comboBox_frec_muestreo.currentIndex()
        self.last_umbral = gui.data_config.ui.doubleSpinBox_umbral.value()

        self.gui.data_config.ui.checkBox_habilitar_media.toggled.connect(lambda: self.request_set_habilitar(0))
        self.gui.data_config.ui.checkBox_habilitar_picos.toggled.connect(lambda: self.request_set_habilitar(1))

        self.gui.data_config.ui.doubleSpinBox_umbral.valueChanged.connect(self.request_set_umbral)

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
    def request_set_envio(self, info: tuple):
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
        self.set_envio(info[1], info[0])

    # recibe la confirmacion de la esp
    def set_envio(self, b: bool, var: int):
        """
        var = 0: temperatura
        var = 1: humedad
        var = 2: presion
        var = 3: media
        var = 4: picos
        """
        boxes = [self.gui.data_config.ui.checkBox_temp, 
                 self.gui.data_config.ui.checkBox_hum, 
                 self.gui.data_config.ui.checkBox_presion]

        graphs = [self.gui.plot_temp.ui.GraphSpace, 
                  self.gui.plot_hum,
                  self.gui.plot_pres,
                  self.gui.plot_temp.ui.label_seccion_media]
        
        boxes[var].blockSignals(True)
        boxes[var].setChecked(b)
        boxes[var].blockSignals(False)

        if b:
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

    def request_set_frecuencia_muestreo(self, v: str):

        print(f"solcitando {v}")
        self.gui.data_config.ui.comboBox_frec_muestreo.blockSignals(True)
        self.gui.data_config.ui.comboBox_frec_muestreo.setCurrentIndex(self.last_frec_index)
        self.gui.data_config.ui.comboBox_frec_muestreo.blockSignals(False)
        self.send_uart()
        self.set_frecuencia_muestreo(v)

    def set_frecuencia_muestreo(self, v: str):

        i = self.gui.data_config.ui.comboBox_frec_muestreo.findText(v)
        self.gui.data_config.ui.comboBox_frec_muestreo.blockSignals(True)
        self.gui.data_config.ui.comboBox_frec_muestreo.setCurrentIndex(i)
        self.gui.data_config.ui.comboBox_frec_muestreo.blockSignals(False)
        self.last_frec_index = i

    def request_set_umbral(self, v: float):

        self.gui.data_config.ui.doubleSpinBox_umbral.blockSignals(True)
        self.gui.data_config.ui.doubleSpinBox_umbral.setValue(self.last_umbral)
        self.gui.data_config.ui.doubleSpinBox_umbral.blockSignals(False)
        self.set_umbral(v)

    def set_umbral(self, v: float):

        self.gui.data_config.ui.doubleSpinBox_umbral.blockSignals(True)
        self.gui.data_config.ui.doubleSpinBox_umbral.setValue(v)
        self.gui.data_config.ui.doubleSpinBox_umbral.blockSignals(False)
        self.last_umbral = v

    def conectar(self):
        pass
