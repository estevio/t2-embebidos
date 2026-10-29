from PyQt6.QtWidgets import QApplication, QWidget, QCheckBox
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

from widgets_ui.conf_datos_ui import Ui_ConfDatos

class AppConfDatos(QWidget):

    def __init__(self,):
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

        # settear opciones umbral
        self.ui.horizontalSlider_umbral.setMaximum(350)
        self.ui.horizontalSlider_umbral.setMinimum(200)
        
        # conectar slider con label
        self.ui.horizontalSlider_umbral.valueChanged.connect(self.slide)

        # agregar opciones ventana
        self.ventanas = [5, 10, 30, 60]
        for v in self.ventanas:
            self.ui.comboBox_media.addItem(str(v))
        self.ui.comboBox_media.setCurrentText("10")

        # forzar eleccion de al menos una variable
        self.ui.checkBox_temp.stateChanged.connect(lambda: self.check_var(0))
        self.ui.checkBox_hum.stateChanged.connect(lambda: self.check_var(1))
        self.ui.checkBox_presion.stateChanged.connect(lambda: self.check_var(2))


    def slide(self, value: int):

        umb = value / 10
        self.ui.label_umbral_picos.setText(f"Umbral (°C):     {str(umb)}")

    def check_var(self, v: int):

        boxes = [self.ui.checkBox_temp, self.ui.checkBox_hum, self.ui.checkBox_presion]

        c = False
        for b in boxes:
            if b.isChecked():
                c = True
                break

        if not c:
            boxes[v].setChecked(True)
        

if __name__ == "__main__":
    app = QApplication([])
    app_con = AppConfDatos()
    app_con.show()
    app.exec()