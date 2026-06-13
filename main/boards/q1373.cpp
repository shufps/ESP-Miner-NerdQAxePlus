#include "q1373.h"
#include "bm1373.h"

Q1373B::Q1373B() : Q1370B()
{
    m_deviceModel = "Q1373";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1373";
    m_asicCount = 4;

    m_asicFrequencies = {300, 325, 350, 375, 400, 425, 450};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1050;

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
