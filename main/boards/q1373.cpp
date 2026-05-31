#include "q1373.h"
#include "bm1373.h"

Q1373B::Q1373B() : Q1370B()
{
    m_deviceModel = "Q1373";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1373";
    m_asicCount = 1;

    m_asicFrequencies = {300, 325, 350, 375, 400, 425, 450};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1050;

#ifdef Q1373
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1373();
}
