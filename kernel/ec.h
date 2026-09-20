#ifndef EC_H
#define EC_H
#include <stdint.h>
#include <stdbool.h>

bool ec_read_byte(uint8_t offset, uint8_t *out_value);

bool ec_write_byte(uint8_t offset, uint8_t value);

#endif
