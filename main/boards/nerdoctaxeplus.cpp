#include "board.h"
#include "nerdoctaxeplus.h"

static const char* TAG="nerdoctaxe+";

NerdOctaxePlus::NerdOctaxePlus() : NerdQaxePlus() {
    this->device_model = "NerdOCTAXE+";
    this->version = 501;
    this->asic_model = "BM1368";
    this->asic_count = 8;
    this->asic_job_frequency_ms = 1500;
    this->asic_frequency = 490.0;
    this->asic_voltage = 1.20;
    this->asic_initial_difficulty = BM1368_INITIAL_DIFFICULTY;
    this->fan_invert_polarity = false;
    this->fan_perc = 100;
}
