#include <stdint.h>
#include <stdbool.h>
#include "io.h"
#include "ec.h"

#define EC_DATA_PORT 0x62
#define EC_SC_PORT   0x66

#define EC_OBF (1 << 0)
#define EC_IBF (1 << 1)

#define EC_CMD_READ  0x80
#define EC_CMD_WRITE 0x81

#define EC_TIMEOUT_ITERS 100000

static bool ec_wait_ibf_clear(void) {
    for (int i = 0; i < EC_TIMEOUT_ITERS; i++) {
        if ((inb(EC_SC_PORT) & EC_IBF) == 0) return true;
        io_wait();
    }
    return false;
}

static bool ec_wait_obf_set(void) {
    for (int i = 0; i < EC_TIMEOUT_ITERS; i++) {
        if (inb(EC_SC_PORT) & EC_OBF) return true;
        io_wait();
    }
    return false;
}

bool ec_read_byte(uint8_t offset, uint8_t *out_value) {
    if (!ec_wait_ibf_clear()) return false;
    outb(EC_SC_PORT, EC_CMD_READ);

    if (!ec_wait_ibf_clear()) return false;
    outb(EC_DATA_PORT, offset);

    if (!ec_wait_obf_set()) return false;
    *out_value = inb(EC_DATA_PORT);
    return true;
}

bool ec_write_byte(uint8_t offset, uint8_t value) {
    if (!ec_wait_ibf_clear()) return false;
    outb(EC_SC_PORT, EC_CMD_WRITE);

    if (!ec_wait_ibf_clear()) return false;
    outb(EC_DATA_PORT, offset);

    if (!ec_wait_ibf_clear()) return false;
    outb(EC_DATA_PORT, value);

    return ec_wait_ibf_clear();
}
