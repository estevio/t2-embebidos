#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <esp_random.h>

// Estructuras para el envío de datos

// mediciones
typedef struct {
    int seq;
    int tiempo;
    float temp;
    float hum;
    float pres;
    float avg;
    int ventana_completa; // booleano
} Medicion;

// picos
typedef struct {
    int tiempo;
    float t_pico; // temperatura del pico
    float umbral; // umbral asociado
} Pico;

// Confirmación de recepción de informacion
typedef struct {
    char status[2]
    float frec_muestreo;
    char habilitar[3];
    int vantana_avg;
    float umbral_pico;
} Ack;

typedef struct {
    float frec_muestreo;
    int habilitadas[5]; // booleanos para T, H, P, avg, pico
    int ventana_avg;
    float umbral_pico;
} Config;

// traducción estructura -> JSONL
char* 

void app_main(void) {

}
