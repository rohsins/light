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
static char deviceAlias[32];
static char deviceColor[9];

static Mqtt* mqttInstance = new Mqtt();

static char* PacketFormat = "{essential:{subscriberudi:\"%s\",payloadType:\"%s\",payload:{thingCode:%d,alias:\"%s\",isChecked:%B,intensity:%d,color:\"%.*s\"}}}";

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

    static struct json_token publisherudi = { NULL, 0, JSON_TYPE_INVALID };
    static struct json_token payloadType = { NULL, 0, JSON_TYPE_INVALID };
    static struct json_token payload = { NULL, 0, JSON_TYPE_INVALID };
    static struct json_token message = { NULL, 0, JSON_TYPE_INVALID };
    static struct json_token details = { NULL, 0, JSON_TYPE_INVALID };
    static struct json_token color = { NULL, 0, JSON_TYPE_INVALID };
    static int thingCode = 0;
    static int isChecked = 0;
    static int intensity = 0;
    static int doorSensor = 0;
    static int motionSensor = 0;
    static int value = 0;
    static bool entered = false;

    json_scanf(event->data, event->data_len, "{ publisherudi: %T, payloadType: %T, payload: %T }", &publisherudi, &payloadType, &payload);
    if (payload.len == 0) {
        struct json_token t;
        // for (int i = 0; json_scanf_array_elem(event->data, event->data_len, "", i, &t) > 0; i++) {
        //     printf("Index %d, token [%.*s]\n", i, t.len, t.ptr);
        json_scanf_array_elem(event->data, event->data_len, "", NULL, &t);
        json_scanf(t.ptr, t.len, "{ publisherudi: %T, payloadType: %T, payload: %T }", &publisherudi, &payloadType, &payload);
        // }
    }
    json_scanf(payload.ptr, payload.len, "{ thingCode: %d, message: %T, details: %T, isChecked: %B, intensity: %d, color: %T}", &thingCode, &message, &details, &isChecked, &intensity, &color);

    if ((thingCode == 12001) && (strncmp(payloadType.ptr, "request", payloadType.len) != 0)) {
        int colorNumber = (int)strtol(color.ptr + 3, NULL, 16);
        if (isChecked) {
            setColor(colorNumber, intensity);
        } else {
            setColor(0x00, 0x00);
        }
        // printf("publisherudi: %.*s\npayloadType: %.*s\npayload: %.*s\n", publisherudi.len, publisherudi.ptr, payloadType.len, payloadType.ptr, payload.len, payload.ptr);
        // printf("thingCode: %d\nisChecked: %d\nintensity: %d\ncolor: %.*s\n", thingCode, isChecked, intensity, color.len, color.ptr);
        if (strncmp(payloadType.ptr, "command", payloadType.len) == 0) {
            int ComposedPacketLength = 0;

            char buf[300] = "";
            struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));

            ComposedPacketLength = json_printf(&jOut, PacketFormat, deviceUdi, devicePayloadType, deviceThingCode, deviceAlias, isChecked, intensity, color.len, color.ptr);
            mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
            memcpy(deviceColor, color.ptr, color.len);
        }
    }
    if (strncmp(payloadType.ptr, "settings", payloadType.len) == 0 && entered == false) {
        entered = true;
        static struct json_token alias = { NULL, 0, JSON_TYPE_INVALID };
        json_scanf(payload.ptr, payload.len, "{ alias: %T }", &alias);
        if (alias.len != 0) {
            strncpy(deviceAlias, alias.ptr, alias.len);
        }
    }
    if (strncmp(payloadType.ptr, "request", payloadType.len) == 0) {
        bool state = false;
        json_scanf(payload.ptr, payload.len, "{ state: %B }", &state);
        if (state) {
            static char* PacketFormat2 = "{essential:{subscriberudi:\"%s\",targetPublisher:[\"%.*s\"],payloadType:\"%s\",payload:{thingCode:%d,alias:\"%s\",isChecked:%B,intensity:%d,color:\"%s\"}}}";
            int ComposedPacketLength = 0;
            char buf[300] = "";
            struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));
            ComposedPacketLength = json_printf(&jOut, PacketFormat2, deviceUdi, publisherudi.len, publisherudi.ptr, "response", deviceThingCode, deviceAlias, isChecked, intensity, deviceColor);
            mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
        }
    }
    if (strncmp(payloadType.ptr, "alert", payloadType.len) == 0) {
        json_scanf(details.ptr, details.len, "{ motionSensor: %d, doorSensor: %d, value: %d }", &motionSensor, &doorSensor, &value);
        if ((value == 1 || motionSensor == 1 || doorSensor == 1) && (intensity < 201 || !isChecked)) {
            int colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
            setColor(colorNumber, 200);
            vTaskDelay(10000);
            if (isChecked) {
                setColor(colorNumber, intensity);
            } else {
                setColor(0x00, 0x00);
            }
        }
        if ((strncmp(message.ptr, "Motion Detected!", message.len) == 0
        || strncmp(message.ptr, "Motion detected!", message.len) == 0
        || strncmp(message.ptr, "Door Opened!", message.len) == 0
        || strncmp(message.ptr, "Door opened!", message.len) == 0)
        && (intensity < 201 || !isChecked)) {
            int colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
            setColor(colorNumber, 200);
            vTaskDelay(10000);
            if (isChecked) {
                setColor(colorNumber, intensity);
            } else {
                setColor(0x00, 0x00);
            }
        }
    }
}

// void mqttTask(void *) {
//     mqttInstance->Init();
//     mqttInstance->Connect();
//     *mqttInstance->Subscribe("RTSR&D/baanvak/sub/lightSwitch00001", 2) = subListener;
//     vTaskSuspend(NULL);
// }

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
