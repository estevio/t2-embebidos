import time
import collections
import random as rnd
from PyQt6.QtWidgets import QApplication, QWidget
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg

from widgets_ui.graph_ui import Ui_Grafico

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
    def __init__(self, titulo: str = "titulo"):
        QWidget.__init__(self)
        self.ui = Ui_Grafico()
        self.ui.setupUi(self)
        self.graf = Grafico()
        self.ui.title.setText(titulo)
        self.ui.graph_layout.addWidget(self.graf)

if __name__ == "__main__":
    app = QApplication([])
    app_graf = AppGrafico("Un gráfico cualquiera")
    app_graf.show()
    app.exec()
