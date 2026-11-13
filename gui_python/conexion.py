import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import QApplication, QWidget
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

from widgets_ui.conexion_ui import Ui_ConfigConexion

class AppConexion(QWidget):

    conectar_toggled = pyqtSignal()

    def __init__(self):
        super().__init__()
        self.ui = Ui_ConfigConexion()
        self.ui.setupUi(self)

        # listar puertos
        self.available_ports = serial.tools.list_ports.comports()

        # añadir opciones de puertos
        self.ui.comboBox_puerto.addItem("Seleccione un puerto")
        for p in self.available_ports:
            if "COM" in p.device or "USB" in p.device:
                self.ui.comboBox_puerto.addItem(p.device)
        self.ui.comboBox_puerto.setCurrentText("Seleccione un puerto")

        # añadir opciones de baud rate
        self.baud_rates = [38400, 57600, 115200, 230400, 460800, 921600]
        for b in self.baud_rates:
            self.ui.comboBox_baud_rate.addItem(str(b))
        self.ui.comboBox_baud_rate.setCurrentText("115200")

        # conectar gui - receiver
        self.ui.pushButton_conectar.pressed.connect(self.request_conectar)

    def request_conectar(self):
        print("enviando señal")
        self.conectar_toggled.emit()

    @pyqtSlot()
    def toggle_conectar(self):
        print("toggle conectar en conexion")
        if self.ui.pushButton_conectar.text() == "Conectar":
            self.ui.pushButton_conectar.setText("Desconectar")
            # bloquar cambio de baud rate + puerto (?)
            self.ui.comboBox_puerto.setDisabled(True)
            self.ui.comboBox_baud_rate.setDisabled(True)
        else:
            self.ui.pushButton_conectar.setText("Conectar")
            self.ui.comboBox_puerto.setDisabled(False)
            self.ui.comboBox_baud_rate.setDisabled(False)

if __name__ == "__main__":
    app = QApplication([])
    app_con = AppConexion()
    app_con.show()
    app.exec()