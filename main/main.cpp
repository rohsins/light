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
#include "./RingBuffer.h"
#include "../../rLibrary/rMqtt/mqtt.h"
#include "../../rLibrary/rWifi/wifi.h"

#include "./frozen.h"
extern "C" {
    #include "./ws2812_control.h"
}

#define TXD_PIN GPIO_NUM_4
#define RXD_PIN GPIO_NUM_5

static const char *TAG = "lightSwitch";
static const int RX_BUF_SIZE = 1024;

static const char * deviceUdi = "lightSwitch00001";
static const char * devicePayloadType = "commandReply";
static const uint16_t deviceThingCode = 13001;

static RingBuffer* ringBuffer = new RingBuffer();
static Mqtt* mqttInstance = new Mqtt();

static char* PacketFormat = "{\"essential\":{\"subscriberudi\":\"%s\",\"payloadType\":\"%s\",\"payload\":{\"thingCode\":%d,\"data\":\"%s\"}}}";
static uint16_t PacketLength = 0;

void setColor(int color) {
    struct led_state led_color;
    for (int i = 0; i < 16; i++) {
        led_color.leds[i] = color;
    }
    ws2812_write_leds(led_color);
}

void subListener(esp_mqtt_event_handle_t event) {
    char * dataStream = (char *)calloc(event->data_len, sizeof(char *));
    memcpy(dataStream, event->data, event->data_len);

    char * essential = (char *)calloc(128, sizeof(char *));
    char * publisherudi = (char *)calloc(17, sizeof(char *));
    char * payloadType = (char *)calloc(12, sizeof(char *));
    char * payload = (char *)calloc(32, sizeof(char *));
    int thingCode = 0;
    int isChecked = 0;
    int intensity = 0;
    char * color = (char *)calloc(128, sizeof(char *));

    // PacketLength = strlen(PacketFormat) + strlen(deviceUdi) + strlen(devicePayloadType) + 512;
    // char* ComposedPacket = (char*)calloc(PacketLength, sizeof(char *));
    // int ComposedPacketLength = 0;

    // json_scanf(dataStream, event->data_len, "{ essential: %Q }", &essential);
    json_scanf(dataStream, event->data_len, "{ publisherudi: %Q, payloadType: %Q, payload: %Q }", &publisherudi, &payloadType, &payload);
    json_scanf(payload, strlen(payload), "{ thingCode: %d, isChecked: %B, intensity: %d, color: %Q}", &thingCode, &isChecked, &intensity, &color);

    char parsedColor[6] = { 0 };
    memcpy(parsedColor, color + 3, 6);
    char correctedColor[6] = { parsedColor[2], parsedColor[3], parsedColor[0], parsedColor[1], parsedColor[4], parsedColor[5] };
    int colorNumber = (int)strtol(correctedColor, NULL, 16);
    printf("\ncolor in int: %X\n", colorNumber);

    if (isChecked) {
        setColor(colorNumber);
    } else {
        setColor(0x00);
    }

    // ComposedPacketLength = snprintf(ComposedPacket, PacketLength, PacketFormat, deviceUdi, devicePayloadType, deviceThingCode, output);
    // mqttInstance->Publish("RTSR&D/baanvak/sub/lightSwitch00001", ComposedPacket, ComposedPacketLength, 2, 0);

    // free(ComposedPacket);

    // if (pin != 0 && (strcmp(action, (const char *)"high")) == 0) {
    //     GPIOHigh(pin);
    // }
    // if (pin != 0 && (strcmp(action, (const char *)"low")) == 0) {
    //     GPIOLow(pin);
    // }
    // if (pin != 0 && (strcmp(action, (const char *)"reset")) == 0) {
    //     ResetCommand(pin);
    // }
    // if (pin != 0 && (strcmp(action, (const char *)"trigger")) == 0) {
    //     TriggerCommand(pin);
    // }
    
    printf("publisherudi: %s\npayloadType: %s\npayload: %s\n", publisherudi, payloadType, payload);
    printf("thingCode: %d\nisChecked: %d\nintensity: %d\ncolor: %s\n", thingCode, isChecked, intensity, color);

    free(dataStream);
    free(essential);
    free(publisherudi);
    free(payloadType);
    free(payload);
    free(color);
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    nvs_flash_init();

    Wifi wifiInstance = Wifi();
    wifiInstance.Init();
    wifiInstance.Connect();

    mqttInstance->Init();
    mqttInstance->Connect();

    *mqttInstance->Subscribe("RTSR&D/baanvak/sub/lightSwitch00001", 2) = subListener;

    ws2812_control_init();

    // xTaskCreate(receiveTask, (const char*)"Receive Task", 2048, (void *)1, 1, NULL);
    // xTaskCreate(runner, (const char*)"runner", 2048, (void *)1, 1, NULL);
}
