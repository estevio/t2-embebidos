import time
import random as rnd
import serial
from queue import Queue
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

class DataReceiver(QObject):

    data_recvd_temp = pyqtSignal(float)
    data_recvd_hum = pyqtSignal(float)
    data_recvd_pres = pyqtSignal(float)
    data_recvd_avg = pyqtSignal(float)
    data_recvd_peak = pyqtSignal(float)

    toggle_conectar = pyqtSignal()
    umbral_changed = pyqtSignal(float)
    frec_changed = pyqtSignal(str)
    toggle_envio = pyqtSignal(int, bool)
    toggle_habilitar = pyqtSignal(int, bool)
    request_full_data_config = pyqtSignal()

    error_msg = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.command_queue = Queue()

        self.port = "Seleccione un puerto"
        self.baud = 115200
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

    # idea: send_uart recibe una señal que debe emitir en caso de que no se esté conectado
    def send_uart(self):

        # revisar si esta conectado
        if not self.conectado:
            return
        # sino, convertir el mensaje a json y añadir un comando que envia la config al esp

    def decode_uart(self):
        pass

    # envía la solicitud a la esp
    @pyqtSlot(int, bool)
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
        # enviar señal a conf_datos y graf
        self.toggle_envio.emit(var, val)

    def request_set_habilitar(self, var: int, val: bool):

        self.set_habilitar(var, val)

    def set_habilitar(self, var: int, b: bool):

        self.toggle_habilitar.emit(var, b)

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

    @pyqtSlot(str)
    def set_port(self, name: str):
        self.port = name

    @pyqtSlot(str)
    def set_baud(self, val: str):
        self.baud = int(val)

    @pyqtSlot()
    def conectar(self):

        def command():
            if self.conectado:
                self.ser.close()
                self.ser = None
                self.conectado = False
                self.toggle_conectar.emit()
            elif self.port == "Seleccione un puerto":
                self.error_msg.emit("Seleccione un purto")
            else:
                try:
                    self.ser = serial.Serial(self.port, self.baud)
                    self.toggle_conectar.emit()
                    self.conectado = True
                except:
                    self.error_msg.emit("Error de conexión")
                    self.ser = None
                    self.conectado = False
        self.command_queue.put(command)

    @pyqtSlot(dict)
    def get_full_data_config(self):

        def command_habilitar():
            pass