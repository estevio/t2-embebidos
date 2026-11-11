#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h> //??
#include "cJSON.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <freertos/semphr.h>

#include <portmacro.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2c_master.h>

#define TAG                     "main"
#define SAMPLE_RATE_HZ          5           // cambiar?

#define BME688_SDA_PIN          33
#define BME688_SCL_PIN          32
#define BME688_I2C_SLAVE_ADDR   0x76

#define BME688_RESET_ADDR       0xE0
#define BME688_CHIP_ID_ADDR     0xD0
#define BME688_CHIP_ID          0x61

#define BME688_CALIB_1_ADDR     0x8A
#define BME688_CALIB_1_LEN      23
#define BME688_CALIB_2_ADDR     0xE1
#define BME688_CALIB_2_LEN      14

#define BME688_CTRL_GAS_0_ADDR  0x70
#define BME688_CTRL_GAS_1_ADDR  0x71
#define BME688_CTRL_HUM_ADDR    0x72
#define BME688_CONFIG_ADDR      0x75

#define BME688_CTRL_MEAS_ADDR   0x74
#define BME688_MEAS_STATUS_0_ADDR   0x1D
#define BME688_DATA_LEN             10  /* 0x1D ... 0x26 */
#define BME688_STATUS_NEW_DATA      (1 << 7)
#define BME688_STATUS_MEASURING     (1 << 5)


#define redondear(v) round((v)*100.0)/100.0


/*
// Estructura que representa un mensaje de configuracion
typedef struct {
    float frec;
    char* habilitar;
    int ventana;
    float umbral;
} ConfigMsg;

ConfigMsg config_from_json(char *json) {

    if (strlen(json) == 0) return;

    cJSON *root = cJSON_Parse(json)
    if (root == NULL) {
        printf("Error al analizar la linea JSON\n");
        return;
    }

    cJSON *tipo = cJSON_GetObjectItem(root, "tipo");
    if (! cJSON_IsString(tipo)) {
        printf("Error al leer JSON: sin tipo\n");
        return;
    }

    ConfigMsg res;
    memset(&res, 0, sizeof(ConfigMsg));

    cJSON *frec = cJSON_GetObjectItem(root, "frec_muestreo");
    if (frec != NULL) {
        if (! cJSON_IsNumber(frec)) {
            printf("Error: frec no es null ni number\n");
            return;
        }
        res.frec = (float) frec->valuedouble;
        return res
    }

    cJSON *habi = cJSON_GetObjectItem(root, "habilitar");
    if (habi != NULL) {
        if (! cJSON_IsString(habi)) {
            printf("Error: habi no es null ni str")
            return;
        }
        res.habilitar = habi->valuestring;
        return res;
    }

    cJSON *vent = cJSON_GetObjectItem(root, "ventana_avg");
    if (vent != NULL) {
        if (! cJSON_IsNumber(vent)) {
            printf("Error: vent no es ni string ni number");
            return;
        }
        res.ventana = vent->valueint;
    }

}
*/

/*  --------------------------------  ESTRUCTURAS  ---------------------------------   */

/* Estructura que contiene los datos obtenidos de la calibracion del sensor */
typedef struct {

    uint16_t par_t1;
    int16_t par_t2;
    int8_t par_t3;

    uint16_t par_p1;
    int16_t par_p2;
    int8_t par_p3;
    int16_t par_p4;
    int16_t par_p5;
    int8_t par_p6;
    int8_t par_p7;
    int16_t par_p8;
    int16_t par_p9;
    uint8_t par_p10;

    uint16_t par_h1;
    uint16_t par_h2;
    int8_t par_h3;
    int8_t par_h4;
    int8_t par_h5;
    uint8_t par_h6;
    int8_t par_h7;

} Calib;

/* Estructura que contiene los datos a conservar luego de la inicializacion del sensor */
typedef struct {
    bool success;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
} ConfigSensor;

/* Estructura que representa un pico de temperatura */
typedef struct {
    bool valid;
    double pico;
    TickType_t ticks;
    double umbral;
} Pico;

/* Estructura que representa un promedio */
typedef struct {
    double avg;
    bool ventana_completa;
} Average;

/* Estructura que contiene mediciones de:
    - temperatura
    - humedad relativa
    - presion
    - promedio de temperaturas
    - pico de temperatura detectado
*/
typedef struct {
    double temp;
    double hum;
    double pres;
    Average avg;
    TickType_t tiempo;
} Medicion;

// Estructura que representa la configuracion actual
typedef struct {
    double frec;
    int ventana;
    double umbral;
    bool h_temp;
    bool h_hum;
    bool h_pres;
    bool h_avg;
    bool h_pico;
    SemaphoreHandle_t m;
} Config;

/* Estructura que representa una cola de temperaturas con ticks asociados */
typedef struct {
    double temps[150];
    TickType_t ticks[150];
    int head;
    int tail;
    double t_sum;
    bool in_peak;
    SemaphoreHandle_t m;
} TQueue;

/* --------------------------------------- VARIABLES GLOBALES ------------------------------------------------ */
SemaphoreHandle_t mutex = NULL;
ConfigSensor *s_conf = NULL;
Calib *cal = NULL;
TQueue *tq = NULL;
Config *gui_conf = NULL;

/* --------------------------------------- FUNCIONES PARA TQUEUE --------------------------------------------- */

/* inicializador de una cola de temperaturas */
void t_queue_init(TQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->t_sum = 0.0;
    q->in_peak = false;
    q->m = xSemaphoreCreateMutex();
}

/* encola temperaturas, despues se debera llamar a _dequeue */
void t_enqueue(TQueue *q, double temp, TickType_t tick) {
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        int pos = (q->tail);
        q->temps[pos] = temp;
        q->ticks[pos] = tick;
        q->tail = (pos + 1) % 150;
        q->t_sum += temp;
        xSemaphoreGive(q->m);
    }
}

/* saca de la cola toda temperatura que haya sido medida antes de ticks */
// ticks debe ser el tiempo de la ultima medicion menos los ticks correspondientes a la ventana actual
void t_dequeue(TQueue *q, TickType_t ticks) {
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        int fst = q->head;
        //printf("fst-ticks: %u\tticks: %u\n", (unsigned int) q->ticks[fst], (unsigned int) ticks);
        while (q->ticks[fst] <= ticks) {
            
            q->t_sum -= q->temps[fst];
            fst++;
            fst %= 150;
        }
        q->head = fst;
        xSemaphoreGive(q->m);
    }
}

/* calcula el promedio de las mediciones dentro de la cola */
Average t_queue_avg(TQueue *q, int expected_size) {
    Average res;
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        int size = q->tail - q->head;
        if (size < 0) size += 150;
        if (size > 0) {
            if (size < expected_size) res.ventana_completa = false;
            else res.ventana_completa = true;
            res.avg = redondear(q->t_sum / (double) size);
        }
        xSemaphoreGive(q->m);
    }
    return res;
}

/* busca picos de temperatura y retorna una estructura pico */
Pico t_queue_peak(TQueue *q, double umbral) {
    Pico res;
    res.valid = false;
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        if (q->tail - q->head >= 3) {
            int pos = q->tail - 1;
            double t1 = q->temps[pos - 2];
            double t2 = q->temps[pos - 1];
            double t3 = q->temps[pos];
            if (q->in_peak) {
                if (t3 < t2) q->in_peak = false;
            }
            else if (t2 >= umbral && t2 > t1 && t2 >= t3) {
                if (t2 > t3) q->in_peak = true;
                res.valid = true;
                res.pico = t2;
                res.ticks = q->ticks[pos - 1];
                res.umbral = umbral;
            }
        }
        xSemaphoreGive(q->m);
    }
    return res;
}

/* -------------------------------- FUNCIONES PARA (DE)INICIALIZACION ------------------------------- */
/* inicializa el sensor, usa las variable globales s_conf y calib */
void init_sensor(void) {

    s_conf->success = true;

    // Init driver
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BME688_SDA_PIN,
        .scl_io_num = BME688_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_conf->bus_handle));
        xSemaphoreGive(mutex);
    }
    

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME688_I2C_SLAVE_ADDR,
        .scl_speed_hz = 400000,
    };
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_bus_add_device(s_conf->bus_handle, &dev_config, &s_conf->dev_handle));
        xSemaphoreGive(mutex);
    }
    

    /* Init a */
    // reiniciar el sensor
    uint8_t buf[2];
    buf[0] = BME688_RESET_ADDR;
    buf[1] = 0xB6;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
        xSemaphoreGive(mutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Init b */
    // verificar conexion
    buf[0] = BME688_CHIP_ID_ADDR;
    uint8_t chip_id = 0;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit_receive(s_conf->dev_handle, buf, 1, &chip_id, 1, -1));
        xSemaphoreGive(mutex);
    }
    
    ESP_LOGI(TAG, "CHIP_ID=0x%hhX\n", chip_id);
    if (chip_id != BME688_CHIP_ID) {
        ESP_LOGE(TAG, "CHIP_ID incorrecto\n");
        s_conf->success = false;
        return;
    }

    /* Init c */
    // calibrar
    uint16_t calib_1_w[(BME688_CALIB_1_LEN + 1) / 2];
    uint8_t *calib_1 = (uint8_t *)calib_1_w;
    buf[0] = BME688_CALIB_1_ADDR;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit_receive(s_conf->dev_handle, buf, 1, calib_1, BME688_CALIB_1_LEN, -1));
        xSemaphoreGive(mutex);
    }

    uint16_t calib_2_w[BME688_CALIB_2_LEN / 2];
    uint8_t *calib_2 = (uint8_t *)calib_2_w;
    buf[0] = BME688_CALIB_2_ADDR;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit_receive(s_conf->dev_handle, buf, 1, calib_2, BME688_CALIB_2_LEN, -1));
        xSemaphoreGive(mutex);
    }

    cal->par_t1 = calib_2_w[4];
    cal->par_t2 = calib_1_w[0];
    cal->par_t3 = calib_1[2];

    cal->par_p1 = calib_1_w[2];
    cal->par_p2 = calib_1_w[3];
    cal->par_p3 = calib_1[8];
    cal->par_p4 = calib_1_w[5];
    cal->par_p5 = calib_1_w[6];
    cal->par_p6 = calib_1[15];
    cal->par_p7 = calib_1[14];
    cal->par_p8 = calib_1_w[9];
    cal->par_p9 = calib_1_w[10];
    cal->par_p10 = calib_1[22];

    cal->par_h1 = (calib_2[2] << 4) | (calib_2[1] & 0x0F);
    cal->par_h2 = (calib_2[0] << 4) | (calib_2[1] >> 4);
    cal->par_h3 = calib_2[3];
    cal->par_h4 = calib_2[4];
    cal->par_h5 = calib_2[5];
    cal->par_h6 = calib_2[6];
    cal->par_h7 = calib_2[7];


    /* Init d */
    // configurar el sensor (oversampling + filter-control)
    buf[0] = BME688_CTRL_HUM_ADDR;
    buf[1] = 0x1;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
        xSemaphoreGive(mutex);
    }
    
    buf[0] = BME688_CONFIG_ADDR;
    buf[1] = 0x00;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
        xSemaphoreGive(mutex);
    }
    
    // desactivar medicion de gas
    buf[0] = BME688_CTRL_GAS_0_ADDR;
    buf[1] = 0x08;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
        xSemaphoreGive(mutex);
    }
    
    buf[0] = BME688_CTRL_GAS_1_ADDR;
    buf[1] = 0x00;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
        xSemaphoreGive(mutex);
    }

}

/* des-inicializa el sensor, usa la variable global s_conf*/
void deinit_sensor(void) {
    /* Deinit driver */
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(s_conf->dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(s_conf->bus_handle));
    ESP_LOGI(TAG, "I2C de-inicializado\n");
}

/* -------------------- FUNCIONES PARA LA RECEPCION, CALCULO Y ENVIO DE MEDICIONES -------------------------- */

/* inicializa todo menos el tiempo */
void innit_medicion(Medicion *m) {
    m->avg.avg = -1000;
    m->hum = -1;
    m->pres = -1;
    m->temp = -1000;
}

/* calcula la conversion de la medicion del sensor, pasandola a las unidades 
   de medida pedidas utilizando la variable global cal 
   y retornando las mediciones solicitadas segun la variable global gui_conf */
Medicion calc_medicion(uint32_t temp_adc, uint32_t press_adc, uint16_t hum_adc, TickType_t ticks) {

    /* Temperature [degC] */
    double var1 = ((double)temp_adc / 16384.0 - (double)cal->par_t1 / 1024.0) * (double)cal->par_t2;
    double d    = (double)temp_adc / 131072.0 - (double)cal->par_t1 / 8192.0;
    double var2 = d * d * ((double)cal->par_t3 * 16.0);
    double t_fine    = var1 + var2;
    double temp_comp = t_fine / 5120.0;

    /* Pressure [Pa] */
    var1 = (t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)cal->par_p6 / 131072.0);
    var2 = var2 + (var1 * (double)cal->par_p5 * 2.0);
    var2 = (var2 / 4.0) + ((double)cal->par_p4 * 65536.0);
    var1 = ((((double)cal->par_p3 * var1 * var1) / 16384.0) + ((double)cal->par_p2 * var1)) / 524288.0;
    var1 = (1.0 + (var1 / 32768.0)) * (double)cal->par_p1;
    double press_comp = 1048576.0 - (double)press_adc;
    if (var1 != 0.0) {
        press_comp = ((press_comp - (var2 / 4096.0)) * 6250.0) / var1;
        var1 = ((double)cal->par_p9 * press_comp * press_comp) / 2147483648.0;
        var2 = press_comp * ((double)cal->par_p8 / 32768.0);
        double var3 = (press_comp / 256.0) * (press_comp / 256.0) * (press_comp / 256.0)
                    * ((double)cal->par_p10 / 131072.0);
        press_comp = press_comp + (var1 + var2 + var3 + ((double)cal->par_p7 * 128.0)) / 16.0;
    } else {
        press_comp = 0.0;
    }

    /* Humidity [%RH] */
    var1 = (double)hum_adc - (((double)cal->par_h1 * 16.0) + (((double)cal->par_h3 / 2.0) * temp_comp));
    var2 = var1 * (((double)cal->par_h2 / 262144.0)
                    * (1.0 + (((double)cal->par_h4 / 16384.0) * temp_comp)
                            + (((double)cal->par_h5 / 1048576.0) * temp_comp * temp_comp)));
    double var3 = (double)cal->par_h6 / 16384.0;
    double var4 = (double)cal->par_h7 / 2097152.0;
    double hum_comp = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);
    if (hum_comp > 100.0)
        hum_comp = 100.0;
    else if (hum_comp < 0.0)
        hum_comp = 0.0;

    Medicion res;
    innit_medicion(&res);
    if (xSemaphoreTake(gui_conf->m, portMAX_DELAY) == pdTRUE) {
        if (gui_conf->h_temp || gui_conf->h_avg || gui_conf->h_pico) {
            t_enqueue(tq, redondear(temp_comp), ticks);
            TickType_t first = ticks - pdMS_TO_TICKS(gui_conf->ventana * 1000);
            if (first > xTaskGetTickCount()) first = 0;
            t_dequeue(tq, first);
            if (gui_conf->h_temp) 
                res.temp = redondear(temp_comp);
            if (gui_conf->h_avg)
                res.avg = t_queue_avg(tq, (int) ceil((float) gui_conf->ventana * gui_conf->frec));
        }
        if (gui_conf->h_pres)
            res.pres = redondear(press_comp);
        if (gui_conf->h_hum)
            res.hum = redondear(hum_comp);
        res.tiempo = ticks;
        xSemaphoreGive(gui_conf->m);
    }
    return res;
}

/* genera un string json_lines a partir de una Medicion */
char *medicion_to_jsonl(Medicion med) {
    cJSON *json_1 = cJSON_CreateObject();
    cJSON *json_2 = cJSON_CreateObject();
    cJSON *json_3 = cJSON_CreateObject();
    char *jsonl = NULL;

    if (cJSON_AddStringToObject(json_1, "tipo", "medicion") == NULL) goto end; // esto es para manejar errores
    int tiempo = med.tiempo * portTICK_PERIOD_MS;
    if (cJSON_AddNumberToObject(json_1, "tiempo", tiempo) == NULL) goto end;

    if (med.temp != -1000){
        if (cJSON_AddNumberToObject(json_2, "temp", med.temp) == NULL) goto end;
    }
    if (med.hum != -1) {
        if (cJSON_AddNumberToObject(json_2, "hum", med.hum) == NULL) goto end;
    }
    if (med.pres != -1) {
        if (cJSON_AddNumberToObject(json_2, "pres", med.pres) == NULL) goto end;
    }

    if (med.avg.avg != -1000) {
        if (cJSON_AddNumberToObject(json_3, "avg", med.avg.avg) == NULL) goto end;
        if (cJSON_AddBoolToObject(json_3, "ventana_completa", med.avg.ventana_completa) == NULL) goto end;
    }

    char *string_1 = cJSON_PrintUnformatted(json_1);
    char *string_2 = cJSON_PrintUnformatted(json_2);
    char *string_3 = cJSON_PrintUnformatted(json_3);

    int len1 = strlen(string_1);
    int len2 = strlen(string_2);
    int len3 = strlen(string_3);

    int len = len1 + len2 + len3;
    jsonl = (char *) malloc(len * sizeof(char));

    if (len1 > 2){
        snprintf(jsonl, (len1) * sizeof(char), string_1);
        strcat(jsonl, "\n");
    } else goto end;
    if (len2 > 2) {
        string_2++;
        strncat(jsonl, string_2, (len2 - 2) * sizeof(char));
        strcat(jsonl, "\n");
    }
    if (len3 > 2) {
        string_3++;
        strncat(jsonl, string_3, (len3 -1) * sizeof(char));
    }

    end:
        cJSON_Delete(json_1);
        cJSON_Delete(json_2);
        cJSON_Delete(json_3);
        return jsonl;

}

/* genera un string json lines a partir de un Pico  */
char *pico_to_jsonl(Pico p) {

    cJSON *json_1 = cJSON_CreateObject();
    cJSON *json_2 = cJSON_CreateObject();
    char *jsonl = NULL;

    if (cJSON_AddStringToObject(json_1, "tipo", "pico") == NULL) goto end;
    int tiempo = p.ticks * portTICK_PERIOD_MS;
    if (cJSON_AddNumberToObject(json_1, "tiempo", tiempo) == NULL) goto end;

    if (cJSON_AddNumberToObject(json_2, "temp_pico", p.pico) == NULL) goto end;
    if (cJSON_AddNumberToObject(json_2, "umbral", p.umbral) == NULL) goto end;

    char *string_1 = cJSON_PrintUnformatted(json_1);
    char *string_2 = cJSON_PrintUnformatted(json_2);

    int len1 = strlen(string_1);
    int len2 = strlen(string_2);

    int len = len1 + len2;
    jsonl = (char *) malloc(len * sizeof(char));

    if (len1 > 2){
        snprintf(jsonl, (len1) * sizeof(char), string_1);
        strcat(jsonl, "\n");
    } else goto end;
    if (len2 > 2) {
        string_2++;
        strncat(jsonl, string_2, (len2 - 1) * sizeof(char));
    }

    end:
        cJSON_Delete(json_1);
        cJSON_Delete(json_2);
        return jsonl;
}

/* funcion que se encarga de recibir mediciones desde el sensor 
   (y TODO: enviarlas a la gui) segun la configuracion gui_conf,
   es una xTask */
void read_data(void *a) {

    /* Read data */
    TickType_t previous_time = xTaskGetTickCount();

    uint8_t buf[2];

    for (uint32_t i = 0; i < 20; i++) {

        printf("%d:\t", (int) i);

        buf[0] = BME688_CTRL_MEAS_ADDR;
        buf[1] = (0x1 << 5) | (0x1 << 2) | 0x1;
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            ESP_ERROR_CHECK(i2c_master_transmit(s_conf->dev_handle, buf, 2, -1));
            xSemaphoreGive(mutex);
        }
        

        uint8_t data[BME688_DATA_LEN];
        bool ready = false;

        for (uint32_t tries = 0; tries < 20; tries++) {
            vTaskDelay(pdMS_TO_TICKS(10));

            buf[0] = BME688_MEAS_STATUS_0_ADDR;
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                ESP_ERROR_CHECK(i2c_master_transmit_receive(s_conf->dev_handle, buf, 1, data, BME688_DATA_LEN, -1));
                xSemaphoreGive(mutex);
            }
            

            if ((data[0] & BME688_STATUS_NEW_DATA) && !(data[0] & BME688_STATUS_MEASURING)) {
                ready = true;
                break;
            }
        }

        if (!ready) {
            ESP_LOGE(TAG, "timeout de medida\n");
        } else {
            uint32_t press_adc = ((uint32_t)data[2] << 12) | ((uint32_t)data[3] << 4) | (data[4] >> 4);
            uint32_t temp_adc  = ((uint32_t)data[5] << 12) | ((uint32_t)data[6] << 4) | (data[7] >> 4);
            uint16_t hum_adc   = (uint16_t)((data[8] << 8) | data[9]);

            Medicion med = calc_medicion(temp_adc, press_adc, hum_adc, previous_time); // es previous time ??
            Pico p;
            p.valid = false;
            if (xSemaphoreTake(gui_conf->m, portMAX_DELAY) == pdTRUE) {
                if (gui_conf->h_pico) p = t_queue_peak(tq, gui_conf->umbral);
                xSemaphoreGive(gui_conf->m);
            }
            char *json_med = medicion_to_jsonl(med);
            printf(json_med);
            printf("\n");
            if (p.valid) {
                char *json_pic = pico_to_jsonl(p);
                printf(json_pic);
                printf("\n");
            }
            
        }
        float frec = 1;
        if (xSemaphoreTake(gui_conf->m, portMAX_DELAY) == pdTRUE) {
            frec = gui_conf->frec;
            xSemaphoreGive(gui_conf->m);
        }
        float ms = 1000.0 / frec;
        xTaskDelayUntil(&previous_time, pdMS_TO_TICKS(ms));
    }
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        deinit_sensor();
        xSemaphoreGive(mutex);
    }
    
    vTaskDelete(NULL);

}

void app_main(void){

    mutex = xSemaphoreCreateMutex();
    s_conf = (ConfigSensor *) malloc(sizeof(ConfigSensor));
    cal = (Calib *) malloc(sizeof(Calib));
    tq = (TQueue *) malloc(sizeof(TQueue));
    t_queue_init(tq);
    gui_conf = (Config *) malloc(sizeof(Config));
    gui_conf->m = xSemaphoreCreateMutex();
    gui_conf->frec = 1;
    gui_conf->umbral = 27.0;
    gui_conf->ventana = 5;
    gui_conf->h_avg = gui_conf->h_pico = true;
    gui_conf->h_temp = gui_conf->h_pres = gui_conf->h_hum = true;

    init_sensor();

    if (s_conf != NULL && s_conf->success == false) return;

    xTaskCreate(read_data, "TaskReadData", 4096, NULL, 2, NULL);

    /*
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        
        xSemaphoreGive(mutex);
    }
    */

    
    //read_data(&s_conf);

    

    // Recibir configuracion de la gui
    // configurar el sensor (?)
    // Enviar confirmacion
}
