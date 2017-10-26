
//#define DS18B20      2 //GPIO2
#define HLW8012_SEL  5 //GPIO5
#define HLW8012_CF  14 //MTMS
#define HLW8012_CF1 13 //MTCK

#include "credentials.h"
#include "capabilities.h"
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266MQTTMesh.h>

#define      FIRMWARE_ID        0x4452
#define      FIRMWARE_VER       "0.8"

const char*  networks[]       = NETWORK_LIST;
const char*  network_password = NETWORK_PASSWORD;
const char*  mesh_password    = MESH_PASSWORD;
const char*  mqtt_server      = MQTT_SERVER;

ESP8266MQTTMesh mesh = ESP8266MQTTMesh::Builder(networks, network_password, mqtt_server)
                     .setVersion(FIRMWARE_VER, FIRMWARE_ID)
                     .setMeshPassword(mesh_password)
                     .build();

bool relayState = false;
int  heartbeat  = 60000;
float temperature = 0.0;

void read_config();
void save_config();
void callback(const char *topic, const char *msg);
String build_json();

void setup() {
    Serial.begin(115200);
    delay(5000);

    mesh.setCallback(callback);
    mesh.begin();

    //mesh.setup will initialize the filesystem
    if (SPIFFS.exists("/config")) {
        read_config();
    }
}

void loop() {
    static unsigned long pressed = 0;
    static unsigned long lastSend = 0;
    static bool needToSend = false;

    unsigned long now = millis();

    if (now - lastSend > heartbeat) {
        needToSend = true;
    }

    if (! mesh.connected()) {
        return;
    }

    if (needToSend) {
        lastSend = now;
        String data = build_json();

        mesh.publish("status", data.c_str());
        needToSend = false;
    }
}

void callback(const char *topic, const char *msg) {
    if (0 == strcmp(topic, "heartbeat")) {
       unsigned int hb = strtoul(msg, NULL, 10);
       if (hb > 10000) {
           heartbeat = hb;
           save_config();
       }
    }
}

String build_json() {
    String msg = "{";
    msg += " \"relay\":\"" + String(relayState ? "ON" : "OFF") + "\"";
    msg += "}";
    return msg;
}

void read_config() {
    File f = SPIFFS.open("/config", "r");
    if (! f) {
        Serial.println("Failed to read config");
        return;
    }
    while(f.available()) {
        char s[32];
        char key[32];
        const char *value;
        s[f.readBytesUntil('\n', s, sizeof(s)-1)] = 0;
        if (! ESP8266MQTTMesh::keyValue(s, '=', key, sizeof(key), &value)) {
            continue;
        }
        else if (0 == strcmp(key, "HEARTBEAT")) {
            heartbeat = atoi(value);
            if (heartbeat < 1000) {
                heartbeat = 1000;
            } else if (heartbeat > 60 * 60 * 1000) {
                heartbeat = 5 * 60 * 1000;
            }
        }
    }
    f.close();
}

void save_config() {
    File f = SPIFFS.open("/config", "w");
    if (! f) {
        Serial.println("Failed to write config");
        return;
    }
    f.print("RELAY=" + String(relayState ? "1" : "0") + "\n");
    f.print("HEARTBEAT=" + String(heartbeat) + "\n");

    f.close();
}
