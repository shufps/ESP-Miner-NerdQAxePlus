#ifndef TPS53647_H_
#define TPS53647_H_

#include <stdint.h>

#define TPS53647_I2CADDR 0x71         //< TPS53647 i2c address

// we use the EN pin
//#define OPERATION_OFF 0x00
//#define OPERATION_ON 0x80

/*-------------------------*/

/* PMBUS_ON_OFF_CONFIG initialization values */
// tps53647 only supports bit 2 and 3
// 3: device doesn't responds to the on_off_operation command
// 2: device uses the enable pin
#define ON_OFF_CONFIG 0b00010111

// minimum voltage the TPS53647 can do
#define HW_MIN_VOLTAGE 0.25

#define TPS53647_INIT_ON_OFF_CONFIG ON_OFF_CONFIG
#define TPS53647_INIT_VOUT_COMMAND 0.75 // 1.165
#define TPS53647_INIT_VOUT_MAX 1.4
#define TPS53647_INIT_VOUT_MARGIN_HIGH 1.1 /* %/100 above VOUT */
#define TPS53647_INIT_VOUT_MARGIN_LOW 0.90 /* %/100 below VOUT */

#define TPS53647_INIT_VOUT_MIN 0.25 // 1.0
#define TPS53647_INIT_VOUT_MAX 1.4

#ifdef NERDQAXEPLUS
    #define TPS53647_INIT_IMAX 60 /* A (int) */
    /* iout current */
    #define TPS53647_INIT_IOUT_OC_WARN_LIMIT 50.00    /* A */
    #define TPS53647_INIT_IOUT_OC_FAULT_LIMIT 55.00   /* A */
#elif defined(NERDQAXEPLUS2)
    #define TPS53647_INIT_IMAX 90 /* A (int) */
    /* iout current */
    #define TPS53647_INIT_IOUT_OC_WARN_LIMIT 80.00    /* A */
    #define TPS53647_INIT_IOUT_OC_FAULT_LIMIT 85.00   /* A */
#elif defined(NERDOCTAXEPLUS)
    #define TPS53647_INIT_IMAX 90 /* A (int) */
    /* iout current */
    #define TPS53647_INIT_IOUT_OC_WARN_LIMIT 80.00    /* A */
    #define TPS53647_INIT_IOUT_OC_FAULT_LIMIT 85.00   /* A */
#else
    /* Default values if none of the conditions match */
    #define TPS53647_INIT_IMAX 60 /* Default A (int) */
    #define TPS53647_INIT_IOUT_OC_WARN_LIMIT 50.00    /* Default A */
    #define TPS53647_INIT_IOUT_OC_FAULT_LIMIT 55.00   /* Default A */
#endif


#define TPS53647_INIT_IOUT_OC_FAULT_RESPONSE 0xC0 /* shut down, no retries */

/* temperature */
// It is better to set the temperature warn limit for TPS546 more higher than Ultra
#define TPS53647_INIT_OT_WARN_LIMIT 105      /* degrees C */
#define TPS53647_INIT_OT_FAULT_LIMIT 145     /* degrees C */
#define TPS53647_INIT_OT_FAULT_RESPONSE 0xFF /* wait for cooling, and retry */

/* public functions */
int TPS53647_init(int num_phases);
int TPS53647_get_frequency(void);
void TPS53647_set_frequency(int);
int TPS53647_get_temperature(void);
float TPS53647_get_vin(void);
float TPS53647_get_iout(void);
float TPS53647_get_iin(void);
float TPS53647_get_vout(void);
float TPS53647_get_pin(void);
float TPS53647_get_pout(void);
void TPS53647_set_vout(float volts);
void TPS53647_show_voltage_settings(void);
void TPS53647_status();

#endif /* TPS53647_H_ */
