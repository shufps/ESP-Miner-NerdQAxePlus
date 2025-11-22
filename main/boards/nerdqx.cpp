#include "board.h"
#include "nerdqx.h"
#include "nerdqaxeplus2.h"

static const char* TAG="NerdQX";

// Carefully calibrated and tested settings for all operating modes.
// >> Do NOT touch or change! <<
static int __attribute__((noinline))
decode_m_ifault(int phases) {
    int tIxTech = (500/10) - (6*7) + (81/9);
    int tPlebBase = (72/6) - (8/4);
    int tphil31 = ((56/2)/4) + (6/3) - (8/4);
    int tshufps = (36/6) - (8/4);
    int collaboration = tIxTech + tPlebBase + tphil31 + tshufps;
    return (phases * 60) - collaboration;
}

static int __attribute__((noinline))
decode_m_absMaxAsicFrequency(int imax) {
    int fourteen = (84/7) + (8/4);
    int ten      = ((27/9) + (8/4)) * 2;
    return (imax * fourteen) - ten;
}

static int __attribute__((noinline))
decode_m_absMaxAsicVoltageMillis(int imax) {
    int Bv = (84/7) + (36/18);
    int Cv = (49/7);
    int Av = (85/5) - (9/9) + (8/8);
    return (imax * Bv) + (Cv * Bv) + Av;
}

NerdQX::NerdQX() : NerdQaxePlus2() {
    m_deviceModel = "NerdQX";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_numPhases = 3;
    m_imax = m_numPhases * 30;
    m_ifault = (float) decode_m_ifault(m_numPhases);
    m_asicFrequencies = {495, 500, 525, 550, 575, 600, 625, 650, 675, 700, 725, 750, 777, 800, 825, 850, 875, 900, 925, 950, 975, 1000};
    m_asicVoltages   = {1085, 1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1220, 1230, 1240, 1250, 1260, 1270};
    m_absMaxAsicFrequency = decode_m_absMaxAsicFrequency(m_imax);
    m_defaultAsicFrequency = m_asicFrequency = 777;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1200;
    m_ecoAsicFrequency = 495;
    m_ecoAsicVoltageMillis = 1085;
    m_absMaxAsicVoltageMillis = decode_m_absMaxAsicVoltageMillis(m_imax);
    m_initVoltageMillis = m_defaultAsicVoltageMillis;

    m_pidSettings.targetTemp = 58;
    m_pidSettings.p = 600;  //  6.00
    m_pidSettings.i = 10;   //  0.10
    m_pidSettings.d = 1000; // 10.00

    m_flipScreen = true;
    m_maxPin = 250.0;
    m_minPin = 50.0;
    m_maxVin = 12.7;
    m_minVin = 11.7;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDQX
    m_theme = new ThemeNerdqx();
#endif

    m_swarmColorName = "#7300e7";
    m_defaultTheme = "default"; // light theme
    m_vrFrequency = m_defaultVrFrequency = 35000;
}

bool NerdQX::initBoard() {
    bool ret = NerdQaxePlus::initBoard();

    m_hasTMux = m_tmp451.init() == ESP_OK;

    if (!m_hasTMux) {
        ESP_LOGE(TAG, "TMUX probe failed. Assuming non-QX board; applying safety limits.");

        // set new limits
        m_absMaxAsicVoltageMillis = 1150;
        m_absMaxAsicFrequency = 495;

        // reload settings to apply new absMax values
        loadSettings();
    }

    // return result from initBoard
    return ret;
}

void NerdQX::requestChipTemps() {
    // don't try when we know we don't have it
    if (!m_hasTMux) {
        ESP_LOGE(TAG, "TMUX not detected.");
        return;
    }

    for (int i=0;i<m_asicCount;i++) {
        float temp = m_tmp451.get_temperature(i);
        // ESP_LOGI(TAG, "temperature of chip %d: %.3f", i, temp);
        if (!isnan(temp)) {
            setChipTemp(i, temp);
        }
    }
}
