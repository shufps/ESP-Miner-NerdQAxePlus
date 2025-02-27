#pragma once

#include "ArduinoJson.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

struct BoardParameters {
    float maxPin;
    float minPin;
    float maxVin;
    float minVin;
    float minAsicShutdownTemp;
    float maxAsicShutdownTemp;
    float minVRShutdownTemp;
    float maxVRShutdownTemp;
    int absMinAsicVoltage;
    int absMaxAsicVoltage;
    int absMinFrequency;
    int absMaxFrequency;
    int defaultAsicVoltage;
    int defaultFrequency;
    int* frequencies = nullptr;
    int* asicVoltages = nullptr;
    size_t freqSize = 0;
    size_t voltSize = 0;

    void setFrequencies(const int* values, size_t size) {
        delete[] frequencies;
        frequencies = new int[size];
        memcpy(frequencies, values, size * sizeof(int));
        freqSize = size;
    }

    void setAsicVoltages(const int* values, size_t size) {
        delete[] asicVoltages;
        asicVoltages = new int[size];
        memcpy(asicVoltages, values, size * sizeof(int));
        voltSize = size;
    }
/*
    String toJson() {
        DynamicJsonDocument doc(1024);  // Adjust size as needed

        doc["maxPIn"] = maxPIn;
        doc["minPIn"] = minPIn;
        doc["maxVin"] = maxVin;
        doc["minVin"] = minVin;
        doc["minAsicShutdownTemp"] = minAsicShutdownTemp;
        doc["maxAsicShutdownTemp"] = maxAsicShutdownTemp;
        doc["minVRShutdownTemp"] = minVRShutdownTemp;
        doc["maxVRShutdownTemp"] = maxVRShutdownTemp;
        doc["absMinAsicVoltage"] = absMinAsicVoltage;
        doc["absMaxAsicVoltage"] = absMaxAsicVoltage;
        doc["absMinFrequency"] = absMinFrequency;
        doc["absMaxFrequency"] = absMaxFrequency;
        doc["defaultAsicVoltage"] = defaultAsicVoltage;
        doc["defaultFrequency"] = defaultFrequency;

        // Convert Frequencies array to JSON
        JsonArray freqArray = doc.createNestedArray("Frequencies");
        for (size_t i = 0; i < FreqSize; i++) {
            freqArray.add(Frequencies[i]);
        }

        // Convert AsicVoltages array to JSON
        JsonArray voltArray = doc.createNestedArray("AsicVoltages");
        for (size_t i = 0; i < VoltSize; i++) {
            voltArray.add(AsicVoltages[i]);
        }

        String jsonString;
        serializeJson(doc, jsonString);
        return jsonString;
    }
*/
};