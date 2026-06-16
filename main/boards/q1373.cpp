#include "q1373.h"
#include "bm1373.h"

Q1373B::Q1373B() : Q1370B()
{
    m_deviceModel = "Q1373";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1373";
    m_asicCount = 4;
    m_numPhases = 4;
    m_imax = 123;
    m_ifault = (float) 160.0f;

    m_maxPin = 180.0;
    m_minPin = 50.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 15.0f;

    m_asicFrequencies = {250, 275, 300, 325, 350, 375, 400, 425, 475, 500, 550};
    m_asicVoltages = {980, 990, 1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070, 1080};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1010;
    m_absMaxAsicFrequency = 700;
    m_absMinAsicVoltageMillis = 900;
    m_absMaxAsicVoltageMillis = 1200;
    m_initVoltageMillis = 1050;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600; //   6.00
    m_pidSettings[0].i = 10;  //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65;  // target temp for vreg
    m_pidSettings[1].p = 600;  //   6.00
    m_pidSettings[1].i = 10;   //   0.10
    m_pidSettings[1].d = 1000; // 10.00

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 512;

#ifdef Q1373
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1373();
}

bool Q1373B::initBoard()
{
    if (!Q1370B::initBoard()) {
        return false;
    }

    if (m_tmp451) {
        m_tmp451->set_temp_cal(1.06f, -25.4f, -25.4f, -25.4f, -25.4f);
    }

    return true;
}
