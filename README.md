# t2-embebidos

## Protocolo

### GUI -> ESP

Envío de configuraciones

``
{"tipo":"config"
  "config": "config-tipo"
  "config-tipo":"config-val}
``

```
{"tipo":"config", (config-tipo):(config-val)}

(config-tipo, config-val):
    ("frec_muestreo", (0.5, 1, 2 o 5))
    ("habilitar",("temp", "hum", "pres", "avg", "pico"))
    ("ventana_avg", (5, 10, 30 o 60))
    ("umbral_pico", (float))
```

### ESP32 -> GUI

Envío de mediciones

```
{"tipo": "medicion","seq":(int),"tiempo":(int),
 "temp":(float),"hum":(float),"pres":(float),
 "avg":(float),"ventana_completa":(bool)}
```

Envío de picos de temperatura

```
{"tipo": "pico","tiempo":(int),
 "t_pico":(float),"umbral":(float)}
```

Confirmación de configuraciones

``
{"tipo":"ack","status":"ok","config":"config-tipo"
  "config-tipo":"config-val}
``

```
{"tipo":"ack","status":("ok" o "na"),(config):(config-val)}

(config, config-val):
    ("frec_muestreo", (0.5, 1, 2 o 5))
    ("habilitar",("tem", "hum", "pre", "avg", "pic"))
    ("ventana_avg", (5, 10, 30 o 60))
    ("umbral_pico", (float))
```

``
{"tipo":"config"
  "config": "config-tipo"
  "config-tipo":"config-val}
``

## TODO

TODO magda
  - conectar sin reiniciar
  - leer desde uart (python)
  - leer desde uart (C) 
  - escribir a uart (pyton) 
  - escribir a uart (C) 
  - implementar cola de comandos
  - manejar overflows ticks 
  - manejar setteo de configuraciones sin haber establecido conexion

TODO galindo
  - config to json (python)
  - ack from json (python)
  - config from json (C)
  - ack to json (C)
  - medida from json (python)
  - pico from json (python)


  - revisar detecto de picos (C)
