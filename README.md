# Tarea 2: Adquisición con BME688 y Procesamiento Local en ESP32
**Curso:** CC5328-1 Sistemas Embebidos y Sensores — FCFM, Universidad de Chile  
**Semestre:** 2026-2  
**Profesor:** Luciano Radrigan | **Auxiliar:** Lucas Llort  

---

## 📹 Video de Demostración
* **Enlace directo al video:** Youtube:

---

## 1. Descripción General y Arquitectura del Sistema

El sistema implementa una estación de adquisición ambiental y procesamiento en el borde (*edge computing*) dividida en tres bloques principales:

1. **Sensor Físico (Bosch BME688):** Conectado al microcontrolador mediante el bus $I^2C$ a $400\text{ kHz}$. Provee mediciones crudas (ADC) de temperatura, humedad relativa y presión atmosférica, además de los coeficientes de calibración almacenados en su memoria no volátil (NVM). El calentador de gas se mantiene deshabilitado (`CTRL_GAS_0 = 0x08`, `CTRL_GAS_1 = 0x00`) para optimizar el tiempo de conversión de $T/H/P$.
2. **Firmware Embebido (ESP32 / ESP-IDF en C):** Utiliza el sistema operativo en tiempo real **FreeRTOS** con dos tareas concurrentes (`TaskReadData` y `TaskReceiveUart`) sincronizadas mediante semáforos mutex (`SemaphoreHandle_t`). Ejecuta las fórmulas de compensación de punto flotante del fabricante, administra una cola circular temporal (`TQueue`) para el cálculo de la media móvil y la detección de máximos locales (picos) de temperatura, y se comunica mediante JSON Lines (JSONL) por UART0.
3. **Aplicación de Escritorio (Python 3 / PyQt6):** Interfaz gráfica no bloqueante basada en una arquitectura Modelo-Vista-Controlador mediante señales y slots (`pyqtSignal` / `pyqtSlot`). La lectura y escritura serial (`pyserial`) se ejecuta en un hilo secundario (`threading.Thread` daemon) acoplado a una cola segura de comandos (`queue.Queue`), mientras que la visualización en tiempo real se realiza mediante `matplotlib` (`FigureCanvasQTAgg`).

---

## 2. Estructura del Repositorio

```text
.
├── esp32_firmware/             # Proyecto ESP-IDF para el ESP32
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c              # Driver I2C BME688, TQueue, tareas FreeRTOS y parser cJSON
├── gui_python/                 # Aplicación de escritorio PyQt6
│   ├── main.py                 # Punto de entrada, instanciación de hilos y conexión de señales
│   ├── gui.py                  # Ventana principal e integración de paneles y popups de error
│   ├── conexion.py             # Widget de control de puerto COM/USB y Baud Rate
│   ├── conf_datos.py           # Widget de configuración de T/H/P, Hz, ventana W y umbral
│   ├── graph.py                # Lienzos de Matplotlib en tiempo real y tabla histórica de picos
│   ├── receiver.py             # Hilo de comunicación UART, colas y serialización JSON
│   ├── main_ui/                # Archivos de interfaz generados por Qt Designer
│   └── widgets_ui/             # Sub-widgets generados por Qt Designer
├── requirements.txt            # Dependencias de Python
└── README.md                   # Documentación del proyecto