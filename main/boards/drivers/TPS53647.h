#ifndef TPS53647_H_
#define TPS53647_H_

#include <stdint.h>

#define TPS53647_I2CADDR 0x71 //< TPS53647 i2c address

// we use the EN pin
//#define OPERATION_OFF 0x00
//#define OPERATION_ON 0x80

// minimum voltage the TPS53647 can do
#define TPS53647_HW_MIN_VOLTAGE 0.25

/* PMBUS_ON_OFF_CONFIG initialization values */
// tps53647 only supports bit 2 and 3
// 3: device doesn't responds to the on_off_operation command
// 2: device uses the enable pin
#define TPS53647_INIT_ON_OFF_CONFIG 0b00010111

#define TPS53647_INIT_VOUT_MIN 1.05 //0.25
#define TPS53647_INIT_VOUT_MAX 1.4

// temperature
#define TPS53647_INIT_OT_WARN_LIMIT 105  // degrees C
#define TPS53647_INIT_OT_FAULT_LIMIT 145 // degrees C

/* public functions */
int TPS53647_init(int num_phases, int imax, float ifault);
int TPS53647_get_frequency(void);
void TPS53647_set_frequency(int);
float TPS53647_get_temperature(void);
float TPS53647_get_vin(void);
float TPS53647_get_iout(void);
float TPS53647_get_iin(void);
float TPS53647_get_vout(void);
float TPS53647_get_pin(void);
float TPS53647_get_pout(void);
void TPS53647_set_vout(float volts);
void TPS53647_show_voltage_settings(void);
void TPS53647_status();
uint16_t TPS53647_get_vout_vid(void);
uint8_t TPS53647_get_status_byte(void);

#endif /* TPS53647_H_ */
