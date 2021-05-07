#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"

#include "../../../rLibrary/rMqtt/mqtt.h"
#include "../../../rLibrary/rWifi/wifi.h"

#include "../../../rLibrary/rLED/led_strip.h"

#include "./frozen.h"

static const char *TAG = "lightSwitch";

static const int lightPin = 13;

static const char * deviceUdi = "lightSwitch00001";
static const char * devicePayloadType = "commandReply";
static const uint16_t deviceThingCode = 13001;
static char deviceAlias[32];
static int deviceIsChecked = 0;
static int deviceIntensity = 10;
static char deviceColor[9] = {'#', 'F', 'F', '0', 'A', 'A', '0', 'F', '3'};

static Mqtt* mqttInstance = new Mqtt();

led_strip_t * led;

static bool firstRun = true;

static char* PacketFormat = "{essential:{subscriberudi:\"%s\",payloadType:\"%s\",payload:{thingCode:%d,isChecked:%B,intensity:%d,color:\"%s\"}}}";
static char* PacketFormatIsChecked = "{essential:{subscriberudi:\"%s\",payloadType:\"%s\",payload:{thingCode:%d,isChecked:%B}}}";
static char* PacketFormatIntensity = "{essential:{subscriberudi:\"%s\",payloadType:\"%s\",payload:{thingCode:%d,intensity:%d}}}";
static char* PacketFormatColor = "{essential:{subscriberudi:\"%s\",payloadType:\"%s\",payload:{thingCode:%d,color:\"%s\"}}}";
static char* PacketFormatStore = "{essential:{publisherudi:self,targetSubscriber:[\"%s\"],mqttPacket:{qos:2, retain: %B},payloadType:store,payload:{isChecked:%B,intensity:%d,color:\"%s\"}}}";

void setColor(uint32_t color, uint32_t intensity) {
    uint32_t red = (((0xFF0000 & color) >> 16) * intensity) / 255;
    uint32_t green = (((0x00FF00 & color) >> 8) * intensity) / 255;
    uint32_t blue = ((0x0000FF & color) * intensity) / 255;
    
    for (int i = 0; i < 16; i++) {
        led->set_pixel(led, i, red, green, blue);
    }
    led->refresh(led, 100);
}

void mqttStore() {
    int ComposedPacketLength = 0;
    char buf[300] = "";
    struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));
    ComposedPacketLength = json_printf(&jOut, PacketFormatStore, deviceUdi, true, deviceIsChecked, deviceIntensity, deviceColor);
    mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
}

void subListener(esp_mqtt_event_handle_t event) {
    struct json_token publisherudi = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token payloadType = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token payload = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token message = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token details = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token color = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token isChecked = { NULL, 0, JSON_TYPE_INVALID };
    struct json_token intensity = { NULL, 0, JSON_TYPE_INVALID };
    int thingCode = 0;
    int doorSensor = 0;
    int motionSensor = 0;
    int value = 0;
    int paramUpdate = 0;
    int ComposedPacketLength = 0;

    static bool entered = false;

    if (strncmp(event->topic, "RTSR&D/baanvak/sub/lightSwitch00001", event->topic_len) == 0) {
        json_scanf(event->data, event->data_len, "{ publisherudi: %T, payloadType: %T, payload: %T }", &publisherudi, &payloadType, &payload);
        if (payload.len == 0 && firstRun) {
            struct json_token t;
            for (int i = 0; json_scanf_array_elem(event->data, event->data_len, "", i, &t) > 0; i++) {
                json_scanf_array_elem(event->data, event->data_len, "", i, &t);
                json_scanf(t.ptr, t.len, "{ publisherudi: %T, payloadType: %T, payload: %T }", &publisherudi, &payloadType, &payload);
                if (strncmp(payloadType.ptr, "store", payloadType.len) == 0 && payload.len != 0) {
                    break;
                }
            }
        }
        json_scanf(payload.ptr, payload.len, "{ thingCode: %d, message: %T, details: %T, isChecked: %T, intensity: %T, color: %T}", &thingCode, &message, &details, &isChecked, &intensity, &color);

        if (strncmp(payloadType.ptr, "store", payloadType.len) == 0 && payload.len != 0 && firstRun) {
            json_scanf(payload.ptr, payload.len, "{ isChecked: %T, intensity: %T, color: %T}", &isChecked, &intensity, &color);
            if (isChecked.len != 0) {
                isChecked.len == 4 ? deviceIsChecked = 1 : deviceIsChecked = 0;
            }
            if (intensity.len != 0) {
                deviceIntensity = (int)strtol(intensity.ptr, NULL, 10);
            }
            if (color.len != 0) {
                memcpy(deviceColor, color.ptr, color.len);
            }
            if (deviceIsChecked) {
                int colorNumber = 0;
                colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
                setColor(colorNumber, deviceIntensity);
            } else {
                setColor(0x00, 0x00);
            }
            
            char buf[300] = "";
            struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));
            ComposedPacketLength = json_printf(&jOut, PacketFormat, deviceUdi, devicePayloadType, deviceThingCode, deviceIsChecked, deviceIntensity, deviceColor);
            mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);

            firstRun = false;

            return;
        }

        if (payload.len != 0 && !firstRun) {

            if (strncmp(payloadType.ptr, "alert", payloadType.len) == 0) {
                setColor(0xff0000, 0xFF);
                vTaskDelay(50);
                setColor(0x00, 0x00);
                vTaskDelay(100);
                setColor(0xff0000, 0xFF);
                vTaskDelay(50);
                if (deviceIsChecked) {
                    int colorNumber = 0;
                    colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
                    setColor(colorNumber, deviceIntensity);
                } else {
                    setColor(0x00, 0x00);
                }
                return;
            }

            if (strncmp(payloadType.ptr, "warning", payloadType.len) == 0) {
                setColor(0xd3c600, 0xFF);
                vTaskDelay(50);
                setColor(0x00, 0x00);
                vTaskDelay(100);
                setColor(0xd3c600, 0xFF);
                vTaskDelay(50);
                if (deviceIsChecked) {
                    int colorNumber = 0;
                    colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
                    setColor(colorNumber, deviceIntensity);
                } else {
                    setColor(0x00, 0x00);
                }
                return;
            }

            if (strncmp(payloadType.ptr, "info", payloadType.len) == 0) {
                setColor(0x85a6c3, 0xFF);
                vTaskDelay(50);
                setColor(0x00, 0x00);
                vTaskDelay(100);
                setColor(0x85a6c3, 0xFF);
                vTaskDelay(50);
                if (deviceIsChecked) {
                    int colorNumber = 0;
                    colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
                    setColor(colorNumber, deviceIntensity);
                } else {
                    setColor(0x00, 0x00);
                }
                return;
            }

            if (strncmp(payloadType.ptr, "request", payloadType.len) != 0) {
                if (isChecked.len != 0) {
                    isChecked.len == 4 ? deviceIsChecked = 1 : deviceIsChecked = 0;
                    paramUpdate = 1;
                } else if (intensity.len != 0) {
                    deviceIntensity = (int)strtol(intensity.ptr, NULL, 10);
                    paramUpdate = 2;
                } else if (color.len != 0) {
                    memcpy(deviceColor, color.ptr, color.len);
                    paramUpdate = 3;
                }

                if (deviceIsChecked) {
                    int colorNumber = 0;
                    colorNumber = (int)strtol(deviceColor + 3, NULL, 16);
                    setColor(colorNumber, deviceIntensity);
                } else {
                    setColor(0x00, 0x00);
                }

                if (strncmp(payloadType.ptr, "command", payloadType.len) == 0) {
                    char buf[300] = "";
                    struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));

                    if (paramUpdate == 1) {
                        ComposedPacketLength = json_printf(&jOut, PacketFormatIsChecked, deviceUdi, devicePayloadType, deviceThingCode, deviceIsChecked);
                        mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
                    } else if (paramUpdate == 2) {
                        ComposedPacketLength = json_printf(&jOut, PacketFormatIntensity, deviceUdi, devicePayloadType, deviceThingCode, deviceIntensity);
                        mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
                    } else if (paramUpdate == 3) {
                        ComposedPacketLength = json_printf(&jOut, PacketFormatColor, deviceUdi, devicePayloadType, deviceThingCode, deviceColor);
                        mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
                    }
                    if (!firstRun) mqttStore();

                    return;
                }
            } else {
                bool state = false;
                json_scanf(payload.ptr, payload.len, "{ state: %B }", &state);
                
                if (state) {
                    static char* PacketFormat2 = "{essential:{subscriberudi:\"%s\",targetPublisher:[\"%.*s\"],payloadType:\"%s\",payload:{thingCode:%d,isChecked:%B,intensity:%d,color:\"%s\"}}}";
                    int ComposedPacketLength = 0;
                    char buf[300] = "";
                    struct json_out jOut = JSON_OUT_BUF(buf, sizeof(buf));
                    ComposedPacketLength = json_printf(&jOut, PacketFormat2, deviceUdi, publisherudi.len, publisherudi.ptr, "response", deviceThingCode, deviceIsChecked, deviceIntensity, deviceColor);
                    mqttInstance->Publish("RTSR&D/baanvak/pub/lightSwitch00001", buf, ComposedPacketLength, 2, 0);
                }

                return;
            }
            
            // if (strncmp(payloadType.ptr, "settings", payloadType.len) == 0 && entered == false) {
            //     entered = true;
            //     static struct json_token alias = { NULL, 0, JSON_TYPE_INVALID };
            //     json_scanf(payload.ptr, payload.len, "{ alias: %T }", &alias);
            //     if (alias.len != 0) {
            //         strncpy(deviceAlias, alias.ptr, alias.len);
            //     }
            // }


            // printf("publisherudi: %.*s\npayloadType: %.*s\npayload: %.*s\n", publisherudi.len, publisherudi.ptr, payloadType.len, payloadType.ptr, payload.len, payload.ptr);
            // printf("thingCode: %d\nisChecked: %d\nintensity: %d\ncolor: %s\n", thingCode, deviceIsChecked, deviceIntensity, deviceColor);
        }
    }
}

extern "C" void app_main() {
    nvs_flash_init();

    led = led_strip_init(lightPin, 16);
    led->clear(led, 100);

    Wifi wifiInstance = Wifi();
    wifiInstance.Init();
    wifiInstance.Connect();

    mqttInstance->Init();
    mqttInstance->Connect();
    *mqttInstance->Subscribe("RTSR&D/baanvak/sub/lightSwitch00001", 2) = subListener;

    vTaskDelay(10000);
    if (firstRun) esp_restart();
}
