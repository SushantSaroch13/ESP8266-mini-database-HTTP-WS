#pragma once
#include <ArduinoJson.h>
#include <string.h>   // for strncpy

struct Record {
  char device[10];   // device name
  int temperature = 0;
  int humidity    = 0;
  int pressure    = 0;
  int id          = 0;
};

// JSON → Binary
void jsonToRecord(const JsonObject &obj, Record &rec, int id) {
    rec.id = id;

    // device (string)
    if (obj["device"].is<const char*>()) {
        strncpy(rec.device, obj["device"], sizeof(rec.device));
        rec.device[sizeof(rec.device) - 1] = '\0'; // ensure null termination
    } else {
        rec.device[0] = '\0'; // default empty string
    }

    // numeric values (with safe defaults)
    if (obj["temperature"].is<int>())
        rec.temperature = obj["temperature"];
    else
        rec.temperature = 0;

    if (obj["humidity"].is<int>())
        rec.humidity = obj["humidity"];
    else
        rec.humidity = 0;

    if (obj["pressure"].is<int>())
        rec.pressure = obj["pressure"];
    else
        rec.pressure = 0;
}

// Binary → JSON
void recordToJson(const Record &rec, JsonDocument &doc) {
  doc["device"]      = rec.device;
  doc["temperature"] = rec.temperature;
  doc["humidity"]    = rec.humidity;
  doc["pressure"]    = rec.pressure;
  doc["id"]          = rec.id;
}