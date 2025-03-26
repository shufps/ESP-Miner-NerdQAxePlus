#include "board.h"
#include "nerdeko.h"
#include "nerdqaxeplus2.h"

static const char* TAG="nerdeko";

NerdEko::NerdEko() : NerdQaxePlus2() {
    m_deviceModel = "NerdEKO";
    m_asicModel   = "BM1370";
    m_version     = 100;
    m_numPhases   = 6;
    m_initVoltageMillis = 0;    // use m_asicVoltage for init

    m_maxVin = 13.0;
    m_minVin = 8.0;

    // 40A per phase
    // m_asicCount         = 11;  // 12
    m_maxPin            = 400.0;
    m_minPin            = 30.0;
    m_imax              = 240;     //R = 6000 / (num_phases * max_current) = 24K9
    // m_ifault            = 220.0;
    // m_asicMaxDifficulty = 8192;
    // m_asicMinDifficulty = 2048;

    // m_asicCount         = 9;
    // m_maxPin            = 300.0;
    // m_minPin            = 30.0;
    // m_imax              = 180;  // change R14 : 33K2
    // m_ifault            = 150.0;
    // m_asicMaxDifficulty = 4096;
    // m_asicMinDifficulty = 1024;

    // m_asicCount         = 6;
    // m_maxPin            = 200.0;
    // m_minPin            = 30.0;
    // m_imax              = 120;  // change R14
    // m_ifault            = 120.0;
    // m_asicMaxDifficulty = 2048;
    // m_asicMinDifficulty = 512;

    m_asicCount         = 3;
    // m_maxPin            = 100.0;
    // m_minPin            = 30.0;
    // m_imax              = 70;   // change R14
    m_ifault            = 60.0;
    m_asicMaxDifficulty = 1024;
    m_asicMinDifficulty = 256;


#ifdef NERDEKO
    m_theme = new ThemeNerdoctaxegamma();
#endif
    m_asics = new BM1370();
}
