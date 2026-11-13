import json
import time
import random as rnd
import serial
from queue import Queue
from PyQt6.QtCore import QObject, pyqtSlot, pyqtSignal

class DataReceiver(QObject):

    data_recvd_temp = pyqtSignal(float)
    data_recvd_hum = pyqtSignal(float)
    data_recvd_pres = pyqtSignal(float)
    data_recvd_avg = pyqtSignal(float, bool)
    data_recvd_peak = pyqtSignal(str, str)
    status_recvd = pyqtSignal(str)

    toggle_conectar = pyqtSignal()
    umbral_changed = pyqtSignal(float)
    frec_changed = pyqtSignal(str)
    ventana_changed = pyqtSignal(str)
    toggle_envio = pyqtSignal(int, bool)
    toggle_habilitar = pyqtSignal(int, bool)
    request_full_data_config = pyqtSignal()

    error_msg = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.command_queue = Queue()

        self.port = "Seleccione un puerto"
        self.baud = 115200
        self.ser: serial.Serial = None
        self.conectado = False

    def receiver_loop(self):
        
        PAQUETE_SIZE = 8
        cach = b""
        
        while True:
            while not self.command_queue.empty():
                command = self.command_queue.get_nowait()
                command()

            if not self.conectado or self.ser is None or not self.ser.is_open:
                cach = b""
                time.sleep(0.05)
                continue

            try:
                waiting = self.ser.in_waiting
                if waiting > 0:
                    # Leer en bloques de PAQUETE_SIZE o lo que haya disponible
                    to_read = PAQUETE_SIZE if waiting >= PAQUETE_SIZE else waiting
                    cach += self.ser.read(to_read)

                    while True:
                        beg = cach.find(b'{"tipo')
                        if beg == -1:
                            # Conservar últimos bytes por si '{"tipo' llegó cortado a la mitad
                            if len(cach) > 16:
                                cach = cach[-8:]
                            break

                        # Buscar el '}' de cierre DESPUÉS de beg
                        end = cach.find(b"}", beg)
                        if end == -1:
                            if beg > 0:
                                cach = cach[beg:]
                            break

                        msg = cach[beg : end + 1]
                        cach = cach[end + 1 :]
                        self.decode_uart(msg.decode("utf-8", errors="ignore"))
                else:
                    time.sleep(0.005)
            except (serial.SerialException, OSError) as e:
                self.conectado = False
                try:
                    self.ser.close()
                except Exception:
                    pass
                self.ser = None
                self.toggle_conectar.emit()
                self.status_recvd.emit("Desconectado (puerto perdido)")
                self.error_msg.emit(f"Se perdió la conexión serial con el ESP32: {e}")
            except Exception as e:
                print(f"Error en receiver loop {e}")

            
    def checksum(self, msg: bytes):
        chk = 0
        for character in msg:
            chk ^= ord(character)
        return chk

    def send_uart(self, msg: str):

        # revisar si esta conectado
        if not self.conectado:
            return
        # sino, añadir un comando que envia la config al esp
        def command():
            msg_to_send = msg.encode()
            #check = self.checksum(msg_to_send)
            #msg_to_send = str(check).encode() + msg_to_send
            try:
                self.ser.write(msg_to_send)
                print(f"mensaje enviado: {msg}")
            except:
                self.error_msg.emit(f"Error al enviar mensaje de configuracion")
        self.command_queue.put(command)

    def decode_uart(self, line: str):
        try:
            print(line)
            data = json.loads(line)
        except json.JSONDecodeError:
            # self.error_msg.emit("Error: JSON invalido recibido")
            print("Error: JSON invalido recibido")
            return

        tipo = data.get("tipo")
        if tipo == "medicion":
            self.medida_from_json(line)
        elif tipo == "pico":
            self.pico_from_json(line)
        elif tipo == "ack":
            self.ack_from_json(line)
        elif tipo == "estado":
            estado = data.get("estado", "desconocido")
            msg = data.get("msg", "")
            self.status_recvd.emit(f"{estado}: {msg}" if msg else estado)
            if "error" in estado:
                self.error_msg.emit(f"ESP32 reporta {estado}: {msg}")

    # envía la solicitud a la esp
    @pyqtSlot(int, bool)
    def request_set_envio(self, var: int, val: bool):
        """
        info (tuple): var, bool
        var = 0: temperatura;
        var = 1: humedad;
        var = 2: presion
        """
        # mientras no haya conexion real
        if not self.conectado:
            self.set_envio(var, val)
            return
        
        nombres = ["temp", "hum", "pres"]
        msg_json = self.config_to_json("habilitar", nombres[var], val)

        def command():
            self.send_uart(msg_json)
        self.command_queue.put(command)

        

    # recibe la confirmacion de la esp
    def set_envio(self, var: int, val: bool):
        """
        var = 0: temperatura
        var = 1: humedad
        var = 2: presion
        """
        # enviar señal a conf_datos y graf
        self.toggle_envio.emit(var, val)

    @pyqtSlot(int, bool)
    def request_set_habilitar(self, var: int, val: bool):
        if not self.conectado:
            self.set_habilitar(var, val)
            return

        nombres = ["avg", "pico"]
        msg_json = self.config_to_json("habilitar", nombres[var], val)

        def command():
            self.send_uart(msg_json)
        self.command_queue.put(command)

        
    def set_habilitar(self, var: int, b: bool):

        self.toggle_habilitar.emit(var, b)

    @pyqtSlot(str)
    def request_set_frecuencia_muestreo(self, v: str):
        if not self.conectado:
            self.set_frecuencia_muestreo(v)
            return
        
        msg_json = self.config_to_json("frec_muestreo", float(v))
        def command():
            self.send_uart(msg_json)
        self.command_queue.put(command)

        

    def set_frecuencia_muestreo(self, v: str):
        #print("llamando a set_frecuencia desde receiver")
        self.frec_changed.emit(v)

    @pyqtSlot(str)
    def request_set_ventana(self, v: str):
        if not self.conectado:
            self.set_ventana(v)
            return
        
        msg_json = self.config_to_json("ventana_avg", int(v))

        def command():
            self.send_uart(msg_json)
        self.command_queue.put(command)

        

    def set_ventana(self, v: str):
        self.ventana_changed.emit(v)

    @pyqtSlot(float)
    def request_set_umbral(self, v: float):
        if not self.conectado:
            self.set_umbral(v)
            return
        
        msg_json = self.config_to_json("umbral", round(float(v), 1))
        def command():
            self.send_uart(msg_json)
        self.command_queue.put(command)

        

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
                if self.ser:
                    self.ser.close()
                self.ser = None
                self.conectado = False
                self.toggle_conectar.emit()
                self.status_recvd.emit("Desconectado")
            elif self.port == "Seleccione un puerto":
                self.error_msg.emit("Seleccione un puerto válido")
            else:
                try:
                    self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
                    self.conectado = True
                    self.toggle_conectar.emit()
                    self.status_recvd.emit(f"Conectado en {self.port} ({self.baud} bps)")
                    self.request_full_data_config.emit()
                except Exception as e:
                    self.error_msg.emit("Error de conexión: {e}")
                    self.ser = None
                    self.conectado = False
                    self.status_recvd.emit("Error de conexión")
        self.command_queue.put(command)
    # From - To JSON

    def config_to_json(self, canal: str, valor, val_bool: bool = None) -> str:
        """
        Convierte un cambio de configuracion a string JSON.
        Ejemplos de uso:
          self.config_to_json("frec_muestreo", 2.0)
          self.config_to_json("ventana_avg", 10)
          self.config_to_json("umbral", 28.0)
          self.config_to_json("habilitar", "temp", True)
        """
        msg = {"tipo": "conf", canal: valor}
        if val_bool is not None:
            msg["val"] = bool(val_bool)
        return json.dumps(msg)

    def ack_from_json(self, json_str: str):
        """Parsea un string JSON de tipo 'ack' y actualiza el canal en la GUI."""
        try:
            data = json.loads(json_str)
        except json.JSONDecodeError:
            return

        if data.get("tipo") != "ack":
            return
        if data.get("status") == "error":
            self.error_msg.emit(f"Comando rechazado por ESP32: {data.get('msg', 'formato inválido')}")
            return

        if "frec_muestreo" in data:
            f = float(data["frec_muestreo"])
            f_str = str(int(f)) if f.is_integer() else str(f)
            self.set_frecuencia_muestreo(f_str)

        elif "ventana_avg" in data:
            self.set_ventana(str(int(data["ventana_avg"])))

        elif "umbral" in data:
            self.set_umbral(float(data["umbral"]))

        elif "habilitar" in data:
            hab = data["habilitar"]
            val = bool(data.get("val", False))
            if hab in ("temp", "hum", "pres"):
                self.set_envio({"temp": 0, "hum": 1, "pres": 2}[hab], val)
            elif hab in ("avg", "pico"):
                self.set_habilitar({"avg": 0, "pico": 1}[hab], val)

    def medida_from_json(self, json_str: str):
        """Parsea el JSON de 'medicion' enviado por el ESP32 y emite los datos."""
        try:
            data = json.loads(json_str)
        except json.JSONDecodeError:
            print("Error: JSON de medicion invalido")
            return

        if data.get("tipo") != "medicion":
            return

        if "temp" in data:
            self.data_recvd_temp.emit(float(data["temp"]))
        if "hum" in data:
            self.data_recvd_hum.emit(float(data["hum"]))
        if "pres" in data:
            self.data_recvd_pres.emit(float(data["pres"]))
        if "temp_avg_c" in data:
            completa = bool(data.get("ventana_completa", True))
            self.data_recvd_avg.emit(float(data["temp_avg_c"]), completa)
        # elif avg in data: ...

    def pico_from_json(self, json_str: str):
        """Parsea el JSON de 'pico' enviado por el ESP32 y emite el evento."""
        try:
            data = json.loads(json_str)
        except json.JSONDecodeError:
            self.error_msg.emit("Error: JSON de pico invalido")
            return

        if data.get("tipo") != "pico":
            return

        if "temp_pico" in data:
            tiempo_str = str(data.get("tiempo", 0))
            tiempo_str = f"{float(tiempo_str)/1000.0:.2f} s"
            umbral_str = f" (umbral {data.get('umbral')} °C)" if "umbral" in data else ""
            temp_str = f"{data['temp_pico']} °C{umbral_str}"
            self.data_recvd_peak.emit(tiempo_str, temp_str)

    @pyqtSlot(dict)
    def get_full_data_config(self, data: dict):

        options = ["temp", "hum", "pres", "avg", "pico"]
        for var in options:
            set_to = var in data["habilitar"]
            msg = self.config_to_json("habilitar", valor=var, val_bool=set_to)
            self.send_uart(msg)

        msg = self.config_to_json("frec_muestreo", data["frec_muestreo"])
        self.send_uart(msg)

        msg = self.config_to_json("umbral_pico", data["umbral_pico"])
        self.send_uart(msg)
