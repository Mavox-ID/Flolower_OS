#ifndef BATTERY_H
#define BATTERY_H
#include <stdbool.h>

bool battery_get_status(int *out_percent, bool *out_charging);

#endif
