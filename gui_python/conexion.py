import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import QApplication, QWidget
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

from widgets_ui.conexion_ui import Ui_ConfigConexion

class AppConexion(QWidget):

    def __init__(self):
        super().__init__()
        self.ui = Ui_ConfigConexion()
        self.ui.setupUi(self)

        # listar puertos
        self.available_ports = serial.tools.list_ports.comports()

        # añadir opciones de puertos
        self.ui.comboBox_puerto.addItem("Seleccione un puerto")
        for p in self.available_ports:
            self.ui.comboBox_puerto.addItem(p.name)
        self.ui.comboBox_puerto.setCurrentText("Seleccione un puerto")

        # añadir opciones de baud rate
        self.baud_rates = [38400, 57600, 115200, 230400, 460800, 921600]
        for b in self.baud_rates:
            self.ui.comboBox_baud_rate.addItem(str(b))
        self.ui.comboBox_baud_rate.setCurrentText("115200")

    def toggle_conectar(self):
        
        if self.ui.pushButton_conectar.text == "Conectar":
            self.ui.pushButton_conectar.setText("Desconectar")
        else:
            self.ui.pushButton_conectar.setText("Conectar")

if __name__ == "__main__":
    app = QApplication([])
    app_con = AppConexion()
    app_con.show()
    app.exec()