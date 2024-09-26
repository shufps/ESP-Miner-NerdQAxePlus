#ifndef CRC_H_
#define CRC_H_

uint8_t crc5(uint8_t *data, uint8_t len);
unsigned short crc16(const unsigned char *buffer, int len);
uint16_t crc16_false(uint8_t *buffer, uint16_t len);

#endif // PRETTY_H_