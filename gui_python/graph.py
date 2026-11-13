import time
import collections
import random as rnd
from PyQt6.QtWidgets import QApplication, QWidget, QTableWidgetItem, QAbstractItemView, QHeaderView
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg

from widgets_ui.graph_ui import Ui_Grafico
from widgets_ui.graph_temp_ui import Ui_Grafico_Temp

class Grafico(FigureCanvasQTAgg):
    """
    Clase que repreenta un gráfico de la GUI

    Attributes:
        time_pts: colección de puntos que representa el tiempo
        data_pts: colección de puntos que representa el valor de las mediciones
        init_time: tiempo de inicialización del gráfico
    """
    def __init__(self):
        super().__init__()

        self.axes = self.figure.subplots()
        self.max_points = 100
        self.time_pts = collections.deque(maxlen = self.max_points)
        self.data_pts = collections.deque(maxlen = self.max_points)
        (self.line, ) = self.axes.plot(self.time_pts, self.data_pts, 'b-')
        self.init_time = time.time()

        #self.drawing_timer = self.new_timer(1000 // 30)
        #self.drawing_timer.add_callback(self.redraw)
        #self.drawing_timer.start()

    def redraw(self):
        """
        Actualiza el gráfico
        """

        self.line.set_data(self.time_pts, self.data_pts)
        self.axes.relim()
        self.axes.autoscale_view()
        self.draw_idle()

    @pyqtSlot(float)
    def add_point(self, point: float):
        """
        Añade un punto por dibujar al gráfico

        Parámetros:
            point (float): el punto a graficar
        """
        self.time_pts.append(time.time() - self.init_time)
        self.data_pts.append(point)
        self.redraw()

class AppGrafico(QWidget):
    def __init__(self, titulo: str = "titulo", unidad: str = ""):
        QWidget.__init__(self)
        self.ui = Ui_Grafico()
        self.ui.setupUi(self)
        self.titulo_base = titulo
        self.unidad = unidad
        self.graf = Grafico()
        self.ui.title.setText(f"{self.titulo_base} [{self.unidad}]")
        self.ui.graph_layout.addWidget(self.graf)

    @pyqtSlot(float)
    def update_point(self, val: float):
        self.ui.title.setText(f"{self.titulo_base}: {val:.2f} {self.unidad}")
        self.graf.add_point(val)

    @pyqtSlot(bool)
    def toggle_visibility(self, b: bool):
        if b:
            self.show()
        else:
            self.hide()

class AppGraficoTemp(QWidget):
    def __init__(self, titulo = "Temperatura"):
        QWidget.__init__(self)
        self.ui = Ui_Grafico_Temp()
        self.ui.setupUi(self)
        self.graf = Grafico()
        self.ui.title.setText(titulo)
        self.ui.graph_layout.addWidget(self.graf)

        # tabla de picos
        self.ui.tableWidget_picos.setColumnCount(2)
        self.ui.tableWidget_picos.setHorizontalHeaderItem(0, QTableWidgetItem("Tiempo"))
        self.ui.tableWidget_picos.setHorizontalHeaderItem(1, QTableWidgetItem("°C"))
        self.ui.tableWidget_picos.horizontalHeader().setStretchLastSection(True)
        self.ui.tableWidget_picos.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.ui.tableWidget_picos.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)

    def addPico(self, tiempo: str, temp: str):
        r = self.ui.tableWidget_picos.rowCount()
        self.ui.tableWidget_picos.insertRow(r)
        self.ui.tableWidget_picos.setItem(r, 0, QTableWidgetItem(tiempo))
        self.ui.tableWidget_picos.setItem(r, 1, QTableWidgetItem(temp))

    @pyqtSlot(float, bool)
    def change_avg(self, v: float, completa: bool):
        estado_ventana = "(ventana completa)" if completa else "(llenando ventana...)"
        self.ui.label_media_2.setText(f"Última media reportada: {v:.2f} °C {estado_ventana}")

    @pyqtSlot(bool)
    def toggle_visibility(self, b: bool):
        if b:
            self.show()
            self.ui.GraphSpace.show()
        else:
            self.ui.GraphSpace.hide()
            if self.ui.label_seccion_media.isHidden() and self.ui.label_seccion_picos.isHidden():
                self.hide()

    def toggle_visibility_temp(self, var: int, b: bool):
        vars = [(self.ui.label_seccion_media, self.ui.label_media_2),
                (self.ui.label_seccion_picos, self.ui.tableWidget_picos)]

        if b:
            self.show()
            for fig in vars[var]:
                fig.show()
        else:
            for fig in vars[var]:
                fig.hide()
            if self.ui.GraphSpace.isHidden() and vars[1-var].isHidden():
                self.hide()

    @pyqtSlot(int, bool)
    def set_habilitar(self, var: int, c: bool):
        items = [(self.ui.label_seccion_media, self.ui.label_media_2),
                 (self.ui.label_seccion_picos, self.ui.tableWidget_picos)]

        if c:
            self.show()
            for it in items[var]:
                it.show()
        else:
            for it in items[var]:
                it.hide()
            if items[1-var][0].isHidden() and self.ui.GraphSpace.isHidden():
                self.hide()

if __name__ == "__main__":
    app = QApplication([])
    app_graf = AppGraficoTemp()
    app_graf.show()
    app.exec()
