from PyQt6.QtWidgets import QApplication, QWidget, QCheckBox
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

from widgets_ui.conf_datos_ui import Ui_ConfDatos

class AppConfDatos(QWidget):

    request_envio = pyqtSignal(int, bool)
    request_umbral = pyqtSignal(float)
    request_frecuencia = pyqtSignal(str)
    envio_changed = pyqtSignal(int, bool)

    def __init__(self):
        super().__init__()
        self.ui = Ui_ConfDatos()
        self.ui.setupUi(self)

        # settear todas las variables true por default
        self.ui.checkBox_temp.setChecked(True)
        self.ui.checkBox_hum.setChecked(True)
        self.ui.checkBox_presion.setChecked(True)
        #self.ui.checkBox_habilitar_picos.setChecked(True)
        #self.ui.checkBox_habilitar_media.setChecked(True)

        # agregar opciones frecuencia de muestreo
        self.frecs_muestreo = [0.5, 1, 2, 5]
        for f in self.frecs_muestreo:
            self.ui.comboBox_frec_muestreo.addItem(str(f))
        self.ui.comboBox_frec_muestreo.setCurrentText("1")

        # agregar opciones ventana
        self.ventanas = [5, 10, 30, 60]
        for v in self.ventanas:
            self.ui.comboBox_media.addItem(str(v))
        self.ui.comboBox_media.setCurrentText("10")

        # forzar eleccion de al menos una variable
        self.ui.checkBox_temp.toggled.connect(lambda: self.check_var(0))
        self.ui.checkBox_hum.toggled.connect(lambda: self.check_var(1))
        self.ui.checkBox_presion.toggled.connect(lambda: self.check_var(2))

        self.last_frec_index = self.ui.comboBox_frec_muestreo.currentIndex()
        self.last_umbral = self.ui.doubleSpinBox_umbral.value()

        self.ui.doubleSpinBox_umbral.valueChanged.connect(self.request_set_umbral)
        self.ui.comboBox_frec_muestreo.currentTextChanged.connect(self.request_set_frecuencia)

    def check_var(self, v: int):

        boxes = [self.ui.checkBox_temp, self.ui.checkBox_hum, self.ui.checkBox_presion]

        c = any(b.isChecked() for b in boxes)

        new_val = boxes[v].isChecked()
        boxes[v].blockSignals(True)
        boxes[v].setChecked(not new_val)
        boxes[v].blockSignals(False)

        if c:
            self.request_envio.emit(v, new_val)

    @pyqtSlot(int, bool)
    def set_envio(self, var: int, val:bool):
        """
        var = 0: temperatura
        var = 1: humedad
        var = 2: presion
        """
        boxes = [self.ui.checkBox_temp, 
                self.ui.checkBox_hum, 
                self.ui.checkBox_presion]
        
        boxes[var].blockSignals(True)
        boxes[var].setChecked(val)
        boxes[var].blockSignals(False)

        """
        if val:
            if var == 0 and self.gui.plot_temp.isHidden():
                self.gui.plot_temp.show()
            graphs[var].show()
        else:
            graphs[var].hide()
            cond = self.gui.data_config.ui.checkBox_habilitar_picos.isChecked() or self.gui.data_config.ui.checkBox_habilitar_media.isChecked()
            if not cond:
                self.gui.plot_temp.hide()
        """

    def request_set_umbral(self, v: float):
        #print("request a set_umbral desde data_config")

        self.ui.doubleSpinBox_umbral.blockSignals(True)
        self.ui.doubleSpinBox_umbral.setValue(self.last_umbral)
        self.ui.doubleSpinBox_umbral.blockSignals(False)
        self.request_umbral.emit(v)

    @pyqtSlot(float)
    def set_umbral(self, v: float):
        #print("llamando a set_umbral desde data_config")
        self.ui.doubleSpinBox_umbral.blockSignals(True)
        self.ui.doubleSpinBox_umbral.setValue(v)
        self.ui.doubleSpinBox_umbral.blockSignals(False)
        self.last_umbral = v

    def request_set_frecuencia(self, v: str):
        #print("request a set_frecuencia desde data_config")
        self.ui.comboBox_frec_muestreo.blockSignals(True)
        self.ui.comboBox_frec_muestreo.setCurrentIndex(self.last_frec_index)
        self.ui.comboBox_frec_muestreo.blockSignals(False)
        self.request_frecuencia.emit(v)

    @pyqtSlot(str)
    def set_frecuencia(self, v: str):
        #print("llamando a set_frecuencia desde data_config")
        i = self.ui.comboBox_frec_muestreo.findText(v)
        self.ui.comboBox_frec_muestreo.blockSignals(True)
        self.ui.comboBox_frec_muestreo.setCurrentIndex(i)
        self.ui.comboBox_frec_muestreo.blockSignals(False)
        self.last_frec_index = i

if __name__ == "__main__":
    app = QApplication([])
    app_con = AppConfDatos()
    app_con.show()
    app.exec()