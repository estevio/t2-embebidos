# t2-embebidos

## Protocolo

### GUI -> ESP

Envío de configuraciones

```
{"tipo":"config", (config):(config-val)}

(config, config-val):
    ("frec_muestreo", (0.5, 1, 2 o 5))
    ("habilitar",("temp", "hum", "pres", "avg", "pico"))
    ("ventana_avg", (5, 10, 30 o 60))
    ("umbral_pico", (float))
```

### ESP32 -> GUI

Recepción de mediciones

```
{"tipo": "medicion","seq":(int),"tiempo":(int),
 "temp":(float),"hum":(float),"pres":(float),
 "avg":(float),"ventana_completa":(bool)}
```

Recepción de picos de temperatura

```
{"tipo": "pico","tiempo":(int),
 "t_pico":(float),"umbral":(float)}
```

Confirmación de configuraciones

```
{"tipo":"ack","status":("ok" o "rech"),(config):(config-val)}

(config, config-val):
    ("frec_muestreo", (0.5, 1, 2 o 5))
    ("habilitar",("temp", "hum", "pres", "avg", "pico"))
    ("ventana_avg", (5, 10, 30 o 60))
    ("umbral_pico", (float))
```
