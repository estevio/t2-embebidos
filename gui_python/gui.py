from conexion import AppConexion
from conf_datos import AppConfDatos
from main_ui.main_ui import Ui_MainWindow
from PyQt6.QtWidgets import QMainWindow, QDialog, QVBoxLayout, QLabel
from graph import *

class GUI(QMainWindow):

    def __init__(self, window: QMainWindow):
        super().__init__()
        self.main = Ui_MainWindow()
        self.main.setupUi(window)

        # agregar configuracion de conexion
        self.conexion_config = AppConexion()
        self.main.conf_layout.addWidget(self.conexion_config)

        # agregar configuracion adquisicion de datos
        self.data_config = AppConfDatos()
        self.main.conf_layout.addWidget(self.data_config)

        # agregar gráficos
        self.plot_temp = AppGraficoTemp()
        self.plot_hum = AppGrafico("Humedad Relativa")
        self.plot_pres = AppGrafico("Presión")

        self.main.graf_layout.addWidget(self.plot_temp)
        self.main.graf_layout.addWidget(self.plot_hum)
        self.main.graf_layout.addWidget(self.plot_pres)

        # esconder secciones picos + media
        self.plot_temp.ui.label_seccion_picos.hide()
        self.plot_temp.ui.label_seccion_media.hide()
        self.plot_temp.ui.label_media_2.hide()
        self.plot_temp.ui.tableWidget_picos.hide()

        self.plot_temp = AppGraficoTemp("Temperatura")
        self.plot_hum = AppGrafico("Humedad Relativa", "%HR")
        self.plot_pres = AppGrafico("Presión", "hPa")

    @pyqtSlot(str)
    def error_popup(self, msg: str):
        self.dlg = QDialog(self)
        self.dlg.setWindowTitle("ERROR")
        self.dlg.setLayout(QVBoxLayout())
        self.dlg.layout().addWidget(QLabel(msg))
        self.dlg.exec()