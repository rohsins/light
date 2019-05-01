#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"

#include "../../rLibrary/rMqtt/mqtt.h"
#include "../../rLibrary/rWifi/wifi.h"

#include "./frozen.h"
extern "C" {
    #include "./ws2812_control.h"
}

static const char *TAG = "lightSwitch";

static const char * deviceUdi = "lightSwitch00001";
static const char * devicePayloadType = "commandReply";
static const uint16_t deviceThingCode = 13001;

static Mqtt* mqttInstance = new Mqtt();

static char* PacketFormat = "{\"essential\":{\"subscriberudi\":\"%s\",\"payloadType\":\"%s\",\"payload\":{\"thingCode\":%d,\"data\":\"%s\"}}}";
static uint16_t PacketLength = 0;

void setColor(uint32_t color, uint32_t intensity) {
    uint32_t red = (((0xFF0000 & color) >> 16) * intensity) / 255;
    uint32_t green = (((0x00FF00 & color) >> 8) * intensity) / 255;
    uint32_t blue = ((0x0000FF & color) * intensity) / 255;
    uint32_t grb = (green << 16 | red << 8 | blue);
    struct led_state led_color = {
        grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb, grb
    };
    ws2812_write_leds(led_color);
}

void subListener(esp_mqtt_event_handle_t event) {
    char * dataStream = (char *)calloc(event->data_len, sizeof(char *));
    memcpy(dataStream, event->data, event->data_len);

    struct json_token publisherudi = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token payloadType = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token payload = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token color = { NULL, 0, JSON_TYPE_INVALID };
    int thingCode = 0;
    int isChecked = 0;
    int intensity = 0;

    if (dataStream != NULL) {

        // PacketLength = strlen(PacketFormat) + strlen(deviceUdi) + strlen(devicePayloadType) + 512;
        // char* ComposedPacket = (char*)calloc(PacketLength, sizeof(char *));
        // int ComposedPacketLength = 0;

        json_scanf(dataStream, event->data_len, "{ publisherudi: %T, payloadType: %T, payload: %T }", &publisherudi, &payloadType, &payload);
        json_scanf(payload.ptr, payload.len, "{ thingCode: %d, isChecked: %B, intensity: %d, color: %T}", &thingCode, &isChecked, &intensity, &color);

        if (thingCode == 12001) {
            int colorNumber = (int)strtol(color.ptr + 3, NULL, 16);
            if (isChecked) {
                setColor(colorNumber, intensity);
            } else {
                setColor(0x00, 0x00);
            }
            // printf("publisherudi: %.*s\npayloadType: %.*s\npayload: %.*s\n", publisherudi.len, publisherudi.ptr, payloadType.len, payloadType.ptr, payload.len, payload.ptr);
            // printf("thingCode: %d\nisChecked: %d\nintensity: %d\ncolor: %.*s\n", thingCode, isChecked, intensity, color.len, color.ptr);
        }

        // ComposedPacketLength = snprintf(ComposedPacket, PacketLength, PacketFormat, deviceUdi, devicePayloadType, deviceThingCode, output);
        // mqttInstance->Publish("RTSR&D/baanvak/sub/lightSwitch00001", ComposedPacket, ComposedPacketLength, 2, 0);

        // free(ComposedPacket);
        
        free(dataStream);
    }
}

void mqttTask(void *) {
    mqttInstance->Init();
    mqttInstance->Connect();
    *mqttInstance->Subscribe("RTSR&D/baanvak/sub/lightSwitch00001", 2) = subListener;
    vTaskSuspend(NULL);
}

extern "C" void app_main() {
    // ESP_LOGI(TAG, "[APP] Startup..");
    // ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    // ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    nvs_flash_init();

    ws2812_control_init();

    Wifi wifiInstance = Wifi();
    wifiInstance.Init();
    wifiInstance.Connect();

    mqttInstance->Init();
    mqttInstance->Connect();
    *mqttInstance->Subscribe("RTSR&D/baanvak/sub/lightSwitch00001", 2) = subListener;

    // xTaskCreatePinnedToCore(mqttTask, (const char*)"MqttTask", 20480, NULL, 2, NULL, 1);
    // xTaskCreate(receiveTask, (const char*)"Receive Task", 2048, (void *)1, 1, NULL);
}
