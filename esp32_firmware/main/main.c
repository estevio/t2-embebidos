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
#include <esp_random.h>

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

#define UART_PORT_NUM           0
#define BUF_SIZE               (1024)


#define redondear(v) round((v)*100.0)/100.0

#define TQUEUE_SIZE 310

// Estructura que representa un mensaje de configuracion
typedef struct {
    bool valid;
    char canal[16];     // "frec_muestreo", "habilitar", "ventana_avg", "umbral"
    float frec;
    char habilitar[10]; // "temp", "hum", "pres", "avg", "pico"
    bool val;
    int ventana;
    float umbral;
} ConfigMsg;

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
    int count;
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

void init_uart() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/* inicializador de una cola de temperaturas */
void t_queue_init(TQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->t_sum = 0.0;
    q->in_peak = false;
    q->m = xSemaphoreCreateMutex();
}

/* encola temperaturas, despues se debera llamar a _dequeue */
void t_enqueue(TQueue *q, double temp, TickType_t tick) {
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        if (q->count == TQUEUE_SIZE){
            q->t_sum -= q->temps[q->head];
            q->head = (q->head + 1) % TQUEUE_SIZE;
            q->count--;
        }
        int pos = (q->tail);
        q->temps[pos] = temp;
        q->ticks[pos] = tick;
        q->tail = (pos + 1) % TQUEUE_SIZE;
        q->t_sum += temp;
        q->count++;
        xSemaphoreGive(q->m);
    }
}

/* saca de la cola toda temperatura que haya sido medida antes de ticks */
// ticks debe ser el tiempo de la ultima medicion menos los ticks correspondientes a la ventana actual
void t_dequeue(TQueue *q, TickType_t ticks) {
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        //printf("fst-ticks: %u\tticks: %u\n", (unsigned int) q->ticks[fst], (unsigned int) ticks);
        while (q->count > 0 && (current_ticks - q->ticks[q->head]) >= window_ticks) {
            
            q->t_sum -= q->temps[q->head];
            q->head = (q->head + 1) % TQUEUE_SIZE;
            q->count--;
        }
        q->head = fst;
        xSemaphoreGive(q->m);
    }
}

/* calcula el promedio de las mediciones dentro de la cola */
Average t_queue_avg(TQueue *q, int expected_size, TickType_t window_ticks) {
    Average res = { .avg = -1000, .ventana_completa = false };;
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        if (q->count > 0) {
            // La ventana es completa si ya transcurrió al menos W desde la primera medición o desde el arranque
            res.ventana_completa = (current_ticks >= window_ticks);
            res.avg = redondear(q->t_sum / (double)q->count);
        }
        xSemaphoreGive(q->m);
    }
    return res;
}

/* busca picos de temperatura y retorna una estructura pico */
Pico t_queue_peak(TQueue *q, double umbral) {
    Pico res = { .valid = false, .pico = 0, .ticks = 0, .umbral = umbral };
    if (xSemaphoreTake(q->m, portMAX_DELAY) == pdTRUE) {
        if (q->count >= 3) {
            
            int i3 = (q->tail - 1 + TQUEUE_SIZE) % TQUEUE_SIZE; // muestra k (actual)
            int i2 = (q->tail - 2 + TQUEUE_SIZE) % TQUEUE_SIZE; // muestra k-1 (candidata)
            int i1 = (q->tail - 3 + TQUEUE_SIZE) % TQUEUE_SIZE; // muestra k-2 (anterior)

            double t1 = q->temps[i1];
            double t2 = q->temps[i2];
            double t3 = q->temps[i3];

            if (q->in_peak) {
                if (t3 != t2) q->in_peak = false;
            } else if (t2 >= umbral && t2 > t1 && t2 >= t3) {
                if (t2 == t3) q->in_peak = true;
                res.valid = true;
                res.pico = t2;
                res.ticks = q->ticks[i2];
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
        TickType_t window_ticks = pdMS_TO_TICKS(gui_conf->ventana * 1000);
        if (gui_conf->h_temp || gui_conf->h_avg || gui_conf->h_pico) {
            t_enqueue(tq, redondear(temp_comp), ticks);
            t_dequeue(tq, first, window_ticks);

            if (gui_conf->h_temp) 
                res.temp = redondear(temp_comp);
            if (gui_conf->h_avg)
                res.avg = t_queue_avg(tq, ticks, window_ticks);
        }
        if (gui_conf->h_pres)
            res.pres = redondear(press_comp/100.0);
        if (gui_conf->h_hum)
            res.hum = redondear(hum_comp);
        res.tiempo = ticks;
        xSemaphoreGive(gui_conf->m);
    }
    return res;
}

/* genera un string json_lines (1 sola linea) a partir de una Medicion */
char *medicion_to_jsonl(Medicion med) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddStringToObject(root, "tipo", "medicion");
    int tiempo = med.tiempo * portTICK_PERIOD_MS;
    cJSON_AddNumberToObject(root, "tiempo", tiempo);

    if (med.temp != -1000) {
        cJSON_AddNumberToObject(root, "temp", med.temp);
    }
    if (med.hum != -1) {
        cJSON_AddNumberToObject(root, "hum", med.hum);
    }
    if (med.pres != -1) {
        cJSON_AddNumberToObject(root, "pres", med.pres);
    }
    if (med.avg.avg != -1000) {
        cJSON_AddNumberToObject(root, "avg", med.avg.avg);
        cJSON_AddBoolToObject(root, "ventana_completa", med.avg.ventana_completa);
    }

    char *jsonl = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return jsonl;
}

/* genera un string json_lines (1 sola linea) a partir de un Pico */
char *pico_to_jsonl(Pico p) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddStringToObject(root, "tipo", "pico");
    int tiempo = p.ticks * portTICK_PERIOD_MS;
    cJSON_AddNumberToObject(root, "tiempo", tiempo);
    cJSON_AddNumberToObject(root, "temp_pico", p.pico);
    cJSON_AddNumberToObject(root, "umbral", p.umbral);

    char *jsonl = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return jsonl;
}

ConfigMsg config_from_json(char *json) {
    ConfigMsg res;
    memset(&res, 0, sizeof(ConfigMsg));
    res.valid = false;

    if (json == NULL || strlen(json) == 0) return res;

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        printf("Error al analizar la linea JSON\n");
        return res;
    }

    cJSON *tipo = cJSON_GetObjectItem(root, "tipo");
    if (!cJSON_IsString(tipo) || strcmp(tipo->valuestring, "conf") != 0) {
        printf("Error al leer JSON: sin tipo conf\n");
        cJSON_Delete(root);
        return res;
    }

    cJSON *frec = cJSON_GetObjectItem(root, "frec_muestreo");
    if (frec != NULL) {
        if (!cJSON_IsNumber(frec)) {
            printf("Error: frec no es number\n");
            cJSON_Delete(root);
            return res;
        }
        strcpy(res.canal, "frec_muestreo");
        res.frec = (float)frec->valuedouble;
        res.valid = true;
        cJSON_Delete(root);
        return res;
    }

    cJSON *habi = cJSON_GetObjectItem(root, "habilitar");
    cJSON *val  = cJSON_GetObjectItem(root, "val");
    if (habi != NULL && val != NULL) {
        if (!cJSON_IsString(habi) || !cJSON_IsBool(val)) {
            printf("Error: habi no es str o val no es bool\n");
            cJSON_Delete(root);
            return res;
        }
        strcpy(res.canal, "habilitar");
        strncpy(res.habilitar, habi->valuestring, sizeof(res.habilitar) - 1);
        res.val = cJSON_IsTrue(val);
        res.valid = true;
        cJSON_Delete(root);
        return res;
    }

    cJSON *vent = cJSON_GetObjectItem(root, "ventana_avg");
    if (vent != NULL) {
        if (!cJSON_IsNumber(vent)) {
            printf("Error: vent no es number\n");
            cJSON_Delete(root);
            return res;
        }
        strcpy(res.canal, "ventana_avg");
        res.ventana = vent->valueint;
        res.valid = true;
        cJSON_Delete(root);
        return res;
    }

    cJSON *umb = cJSON_GetObjectItem(root, "umbral");
    if (umb != NULL) {
        if (!cJSON_IsNumber(umb)) {
            printf("Error: umbral no es number\n");
            cJSON_Delete(root);
            return res;
        }
        strcpy(res.canal, "umbral");
        res.umbral = (float)umb->valuedouble;
        res.valid = true;
        cJSON_Delete(root);
        return res;
    }

    cJSON_Delete(root);
    return res;
}

char *ack_to_json(ConfigMsg cfg) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddStringToObject(root, "tipo", "ack");

    if (!cfg.valid) {
        cJSON_AddStringToObject(root, "status", "error");
        char *err_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        return err_str;
    }

    // Actualizar gui_conf protegiendo con su mutex
    if (xSemaphoreTake(gui_conf->m, portMAX_DELAY) == pdTRUE) {
        if (strcmp(cfg.canal, "frec_muestreo") == 0) {
            gui_conf->frec = cfg.frec;
            cJSON_AddNumberToObject(root, "frec_muestreo", cfg.frec);
        } else if (strcmp(cfg.canal, "ventana_avg") == 0) {
            gui_conf->ventana = cfg.ventana;
            cJSON_AddNumberToObject(root, "ventana_avg", cfg.ventana);
        } else if (strcmp(cfg.canal, "umbral") == 0) {
            gui_conf->umbral = redondear(cfg.umbral);
            cJSON_AddNumberToObject(root, "umbral", gui_conf->umbral);
        } else if (strcmp(cfg.canal, "habilitar") == 0) {
            if (strcmp(cfg.habilitar, "temp") == 0) gui_conf->h_temp = cfg.val;
            else if (strcmp(cfg.habilitar, "hum") == 0) gui_conf->h_hum = cfg.val;
            else if (strcmp(cfg.habilitar, "pres") == 0) gui_conf->h_pres = cfg.val;
            else if (strcmp(cfg.habilitar, "avg") == 0) gui_conf->h_avg = cfg.val;
            else if (strcmp(cfg.habilitar, "pico") == 0) gui_conf->h_pico = cfg.val;

            cJSON_AddStringToObject(root, "habilitar", cfg.habilitar);
            cJSON_AddBoolToObject(root, "val", cfg.val);
        }
        xSemaphoreGive(gui_conf->m);
    }

    cJSON_AddStringToObject(root, "status", "ok");
    char *jsonl = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return jsonl;
}

/* funcion que se encarga de recibir mediciones desde el sensor 
   (y TODO: enviarlas a la gui) segun la configuracion gui_conf,
   es una xTask */
void read_data(void *a) {

    /* Read data */
    TickType_t previous_time = xTaskGetTickCount();

    uint8_t buf[2];

    //for (uint32_t i = 0; i < 100; i++) {
    while (true) {

        //printf("%d:\t", (int) i);

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
            uart_write_bytes(UART_PORT_NUM, json_med, sizeof(char)*strlen(json_med));

            if (p.valid) {
                char *json_pic = pico_to_jsonl(p);
                uart_write_bytes(UART_PORT_NUM, json_pic, sizeof(char)*strlen(json_pic));
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

ConfigMsg *find_config_msg(char* msg) {
    int max_len = strlen(msg);
    int beg = strcspn(msg, "{");
    int end = strcspn(msg, "}");
    ConfigMsg *ret = malloc(sizeof(ConfigMsg));
    if (beg == max_len) {
        msg[0] = '\0';
        ret->valid = false;
        return ret;
    }
    else if (end == max_len) {
        if (beg > 0) {
            char aux[max_len];
            strncpy(aux, (msg + beg), max_len);
            strncpy(msg, aux, max_len);
        }
        ret->valid = false;
        return ret;
    }
    else {
        char aux[max_len];
        strncpy(aux, (msg + beg), max_len);
        strncpy(msg, aux, (end - beg + 1));
        ret->valid = true;
        return ret;
    }
    
}

/*
typedef struct {
    bool valid;
    char canal[16];     // "frec_muestreo", "habilitar", "ventana_avg", "umbral"
    float frec;
    char habilitar[10]; // "temp", "hum", "pres", "avg", "pico"
    bool val;
    int ventana;
    float umbral;
} ConfigMsg;
 */

bool change_config(ConfigMsg *conf) {
    if (!conf->valid) {
        return false;
    }
    bool ret_val = false;
    if (xSemaphoreTake(gui_conf->m, portMAX_DELAY) == pdTRUE) {

        if (strncmp(conf->canal, "frec_muestreo", 16)) {
            gui_conf->frec = conf->frec;
            ret_val = true;
        }
        else if (strncmp(conf->canal, "ventana_avg", 16)) {
            gui_conf->ventana = conf->ventana;
            ret_val = true;
        }
        else if (strncmp(conf->canal, "umbral", 16)) {
            gui_conf->umbral = conf->umbral;
            ret_val = true;
        }
        else if (strncmp(conf->canal, "habilitar", 16)) {
            char *var = conf->habilitar;
            bool val = conf->val;
            if (strncmp(var, "temp", 10)) {
                gui_conf->h_temp = val;
                ret_val = true;
            }
            if (strncmp(var, "pres", 10)) {
                gui_conf->h_pres = val;
                ret_val = true;
            }
            if (strncmp(var, "hum", 10)) {
                gui_conf->h_hum = val;
                ret_val = true;
            }
            if (strncmp(var, "avg", 10)) {
                gui_conf->h_avg = val;
                ret_val = true;
            }
            if (strncmp(var, "pico", 10)) {
                gui_conf->h_pico = val;
                ret_val = true;
            }
        }

        xSemaphoreGive(gui_conf->m);
    }
    return ret_val;
}

void receive_uart(void *a) {

    int cach_size = 256;
    int data_size = cach_size/2;

    char *cache = (char *) malloc(cach_size*sizeof(char));
    cache[0] = '\0';
    uint8_t *data = (uint8_t *) malloc(data_size);

    while (true) {
        
        int len = uart_read_bytes(UART_PORT_NUM, data, (data_size-1), pdMS_TO_TICKS(100));

        if (len > 0) {
            data[len] = '\0';
            char *texto = (char *) data;
            // if (strlen(texto) >= cach_size - strlen(cache))  // reacomodar cache
            strncat(cache, texto, 255);

            ConfigMsg *conf = (ConfigMsg *) malloc(sizeof(ConfigMsg));
            conf = find_config_msg(cache);
            bool cond = conf->valid;
            
            while (cond) {
                // modificar configuracion
                conf = find_config_msg(cache);
                cond = conf->valid;
                conf->valid = change_config(conf);
                char *ack =  ack_to_json(*conf);
                uint8_t *ack_bytes = (uint8_t *) ack;
                if (ack != NULL) {
                    uart_write_bytes(UART_PORT_NUM, ack_bytes, strlen(ack) * sizeof(char));
                }
            }
        }
    }
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

    init_uart();
    init_sensor();

    if (s_conf != NULL && s_conf->success == false) return;

    xTaskCreate(read_data, "TaskReadData", 4096, NULL, 2, NULL);
    xTaskCreate(receive_uart, "TaskReceiveUart", 4096, NULL, 2, NULL);
}
