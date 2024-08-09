#ifndef TPS53647_H_
#define TPS53647_H_

#define TPS53647_I2CADDR         0x71  //< TPS53647 i2c address
#define TPS53647_MANUFACTURER_ID 0xFE  //< Manufacturer ID
#define TPS53647_REVISION        0xFF  //< Chip revision

/*-------------------------*/
/* These are the inital values for the voltage regulator configuration */
/* when the config revision stored in the TPS53647 doesn't match, these values are used */


#define OPERATION_OFF 0x00
#define OPERATION_ON  0x80

/*-------------------------*/

/* PMBUS_ON_OFF_CONFIG initialization values */
// tps53647 only supports bit 2 and 3
// 3: device responds to the on_off_operation command
// 2: device ignores enable pin
#define ON_OFF_CONFIG 0b00011011

#define HW_MIN_VOLTAGE 0.50 // hardware limit 0.5V in x100 FP format

#define TPS53647_INIT_ON_OFF_CONFIG ON_OFF_CONFIG
#define TPS53647_INIT_VOUT_COMMAND 1.165
#define TPS53647_INIT_VOUT_MAX 1.4
#define TPS53647_INIT_VOUT_MARGIN_HIGH 1.1 /* %/100 above VOUT */
#define TPS53647_INIT_VOUT_MARGIN_LOW 0.90 /* %/100 below VOUT */

#define TPS53647_INIT_VOUT_MIN 1.0
#define TPS53647_INIT_VOUT_MAX 1.4

  /* iout current */
#define TPS53647_INIT_IOUT_OC_WARN_LIMIT  25.00 /* A */
#define TPS53647_INIT_IOUT_OC_FAULT_LIMIT 30.00 /* A */
#define TPS53647_INIT_IOUT_OC_FAULT_RESPONSE 0xC0  /* shut down, no retries */

  /* temperature */
// It is better to set the temperature warn limit for TPS546 more higher than Ultra
#define TPS53647_INIT_OT_WARN_LIMIT  105 /* degrees C */
#define TPS53647_INIT_OT_FAULT_LIMIT 145 /* degrees C */
#define TPS53647_INIT_OT_FAULT_RESPONSE 0xFF /* wait for cooling, and retry */

/* public functions */
int TPS53647_init(void);
void TPS53647_read_mfr_info(uint8_t *);
void TPS53647_set_mfr_info(void);
void TPS53647_write_entire_config(void);
int TPS53647_get_frequency(void);
void TPS53647_set_frequency(int);
int TPS53647_get_temperature(void);
float TPS53647_get_vin(void);
float TPS53647_get_iout(void);
float TPS53647_get_vout(void);
float TPS53647_get_pin(void);
void TPS53647_set_vout(float volts);
void TPS53647_show_voltage_settings(void);

#endif /* TPS53647_H_ */
