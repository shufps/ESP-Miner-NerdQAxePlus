#include <iostream>
#include <cstring>
#include <cstdlib>

class BoardParameters {
protected:
    float m_minAsicShutdownTemp;
    float m_maxAsicShutdownTemp;
    float m_minVRShutdownTemp;
    float m_maxVRShutdownTemp;
    int m_absMinAsicVoltage;
    int m_absMaxAsicVoltage;
    int m_absMinFrequency;
    int m_absMaxFrequency;
    int m_defaultAsicVoltage;
    int m_defaultFrequency;
    int* m_Frequencies;
    int* m_AsicVoltages;
    size_t m_FreqSize;
    size_t m_VoltSize;

public:
    // Constructor
    BoardParameters() : m_Frequencies(nullptr), m_AsicVoltages(nullptr), m_FreqSize(0), m_VoltSize(0) {}

    // Destructor
    ~BoardParameters() {
        delete[] m_Frequencies;
        delete[] m_AsicVoltages;
    }

    // Getters
    float getMinAsicShutdownTemp() const { return m_minAsicShutdownTemp; }
    float getMaxAsicShutdownTemp() const { return m_maxAsicShutdownTemp; }
    float getMinVRShutdownTemp() const { return m_minVRShutdownTemp; }
    float getMaxVRShutdownTemp() const { return m_maxVRShutdownTemp; }
    int getAbsMinAsicVoltage() const { return m_absMinAsicVoltage; }
    int getAbsMaxAsicVoltage() const { return m_absMaxAsicVoltage; }
    int getAbsMinFrequency() const { return m_absMinFrequency; }
    int getAbsMaxFrequency() const { return m_absMaxFrequency; }
    int getDefaultAsicVoltage() const { return m_defaultAsicVoltage; }
    int getDefaultFrequency() const { return m_defaultFrequency; }
    const int* getFrequencies() const { return m_Frequencies; }
    size_t getFreqSize() const { return m_FreqSize; }
    const int* getAsicVoltages() const { return m_AsicVoltages; }
    size_t getVoltSize() const { return m_VoltSize; }

    // Setters
    void setMinAsicShutdownTemp(float value) { m_minAsicShutdownTemp = value; }
    void setMaxAsicShutdownTemp(float value) { m_maxAsicShutdownTemp = value; }
    void setMinVRShutdownTemp(float value) { m_minVRShutdownTemp = value; }
    void setMaxVRShutdownTemp(float value) { m_maxVRShutdownTemp = value; }
    void setAbsMinAsicVoltage(int value) { m_absMinAsicVoltage = value; }
    void setAbsMaxAsicVoltage(int value) { m_absMaxAsicVoltage = value; }
    void setAbsMinFrequency(int value) { m_absMinFrequency = value; }
    void setAbsMaxFrequency(int value) { m_absMaxFrequency = value; }
    void setDefaultAsicVoltage(int value) { m_defaultAsicVoltage = value; }
    void setDefaultFrequency(int value) { m_defaultFrequency = value; }

    void setFrequencies(const int* values, size_t size) {
        delete[] m_Frequencies;
        m_Frequencies = new int[size];
        memcpy(m_Frequencies, values, size * sizeof(int));
        m_FreqSize = size;
    }

    void setAsicVoltages(const int* values, size_t size) {
        delete[] m_AsicVoltages;
        m_AsicVoltages = new int[size];
        memcpy(m_AsicVoltages, values, size * sizeof(int));
        m_VoltSize = size;
    }
};