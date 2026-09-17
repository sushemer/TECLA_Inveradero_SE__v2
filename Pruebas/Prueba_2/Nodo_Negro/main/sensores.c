#include "sensores.h"

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"


// CONFIGURACIÓN
// GPIO34 = ADC1_CHANNEL_6 en ESP32
#define ADC_CHANNEL       ADC_CHANNEL_6
// Pin de datos del DHT22
#define DHT_PIN           GPIO_NUM_18
// Tiempo máximo de espera para cambios de nivel del DHT22
#define MICRO_SEGUNDOS    200

static const char *TAG = "SENSORES";

// Handle del ADC
static adc_oneshot_unit_handle_t adc_handle;

// ADC
static int leer_adc(void){
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle,ADC_CHANNEL,&raw);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,"Error leyendo ADC: %s",esp_err_to_name(ret));
        return -1;
    }
    return raw;
}


// DHT22
/*
 * Espera hasta que el pin alcance el nivel indicado.
 * Retorna:
 *  >= 0 : tiempo transcurrido en microsegundos
 *  -1   : timeout
 */
static int dht_wait_for_level( gpio_num_t pin, int level,int timeout_us){
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}


//Lee temperatura y humedad directamente del DHT22.
static esp_err_t dht22_read_raw( gpio_num_t pin,float *temperature, float *humidity){
    uint8_t data[5] = {0};
    // SEÑAL DE INICIO
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);

    // Mantener LOW por al menos 1 ms
    esp_rom_delay_us(2000);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_pullup_en(pin);

    // RESPUESTA INICIAL DEL DHT22
    if (dht_wait_for_level(pin, 0,MICRO_SEGUNDOS) < 0) {
        ESP_LOGW( TAG, "DHT22 timeout: LOW inicial");
        return ESP_ERR_TIMEOUT;
    }

    if (dht_wait_for_level(pin,1, MICRO_SEGUNDOS ) < 0) {
        ESP_LOGW(TAG,"DHT22 timeout: HIGH de respuesta"  );
        return ESP_ERR_TIMEOUT;
    }

    if (dht_wait_for_level( pin, 0,MICRO_SEGUNDOS) < 0) {
        ESP_LOGW(TAG,"DHT22 timeout: inicio de datos");
        return ESP_ERR_TIMEOUT;
    }


    // LEER LOS 40 BITS

    for (int i = 0; i < 40; i++) {
        // Cada bit inicia con aproximadamente 50 us LOW.
        // Esperamos el inicio del HIGH.
        if (dht_wait_for_level(pin,1,MICRO_SEGUNDOS ) < 0) {
            ESP_LOGW(TAG,"DHT22 timeout esperando HIGH en bit %d",i );
            return ESP_ERR_TIMEOUT;
        }

        /*
         * Aproximadamente:
         * Bit 0:
         * HIGH ~26 us
         *
         * Bit 1:
         * HIGH ~70 us
         *
         * Después de 40 us:
         * - Si está LOW  -> bit 0
         * - Si sigue HIGH -> bit 1
         */
        esp_rom_delay_us(40);
        int bit = gpio_get_level(pin);

        // Desplazar byte actual
        data[i / 8] <<= 1;
        // Guardar bit si es 1
        if (bit) {
            data[i / 8] |= 1;
        }
        // Si sigue HIGH significa que fue un 1.
        // Esperando a que termine antes del siguiente bit.
        if (i < 39 && bit) {
            if (dht_wait_for_level( pin,0, MICRO_SEGUNDOS) < 0) {
                ESP_LOGW(TAG,"DHT22 timeout terminando bit %d",i  );
                return ESP_ERR_TIMEOUT;
            }
        }
    }


    // CHECKSUM
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4]) {
        ESP_LOGW( TAG, "DHT22 checksum incorrecto");
        return ESP_ERR_INVALID_CRC;
    }

    // HUMEDAD
    uint16_t raw_humidity = ((uint16_t)data[0] << 8) | data[1];
    *humidity = raw_humidity / 10.0f;
    // TEMPERATURA
    uint16_t raw_temperature = ((uint16_t)(data[2] & 0x7F) << 8) | data[3];
    *temperature =raw_temperature / 10.0f;

    // Bit de signo
    if (data[2] & 0x80) {
        *temperature *= -1.0f;
    }
    return ESP_OK;
}


// VPD
static float calcular_vpd( float temperatura, float humedad)
{
    // Presión de vapor de saturación. Resultado en kPa.
    float svp =0.6108f * expf( (17.27f * temperatura) /
            (temperatura + 237.3f));

    // VPD del aire: VPD = SVP * (1 - HR/100)
    float vpd =svp * (1.0f - humedad / 100.0f);
    return vpd;
}


// INICIALIZACIÓN
esp_err_t sensores_init(void){
    // Configurar unidad ADC1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret =adc_oneshot_new_unit(&init_config,&adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,"Error inicializando ADC: %s",esp_err_to_name(ret));
        return ret;
    }

    // Configurar canal ADC
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret =adc_oneshot_config_channel(adc_handle,ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando ADC: %s", esp_err_to_name(ret) );
        return ret;
    }

    ESP_LOGI(TAG, "Sensores inicializados");
    return ESP_OK;
}


// LECTURA GENERAL
void sensores_read(node_packet_t *packet){
    if (packet == NULL) {
        ESP_LOGE(TAG,"Puntero de paquete invalido");
        return;
    }

    // ADC
    packet->adc_raw = leer_adc();
    if (packet->adc_raw >= 0) {
        //Conversión aproximada.
        packet->voltage = ((float)packet->adc_raw / 4095.0f) * 3.3f;
    }
    else {
        packet->voltage = -1.0f;
    }

    // DHT22
    float temperatura = 0.0f;
    float humedad = 0.0f;

    esp_err_t dht_result =dht22_read_raw(DHT_PIN, &temperatura, &humedad);

    if (dht_result == ESP_OK) {
        packet->temperature = temperatura;
        packet->humidity = humedad;

        // VPD
        //  Solo se calcula VPD si la humedad recibida está dentro de un rango físicamente válido.
        if (humedad >= 0.0f && humedad <= 100.0f) {
            packet->vpd = calcular_vpd(temperatura,humedad );
            packet->dht_ok = true;
        }
        else {
            ESP_LOGW(TAG,"Humedad fuera de rango: %.1f %%", humedad );
            packet->vpd = 0.0f;
            packet->dht_ok = false;
        }
    }
    else {
        ESP_LOGW( TAG, "Error DHT22: %s",esp_err_to_name(dht_result));
        packet->temperature = 0.0f;
        packet->humidity = 0.0f;
        packet->vpd = 0.0f;
        packet->dht_ok = false;
    }
}