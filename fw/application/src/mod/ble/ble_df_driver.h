#ifndef BLE_DF_DRIVER_H
#define BLE_DF_DRIVER_H

#include <stddef.h>

#include "df_driver.h"

df_driver_t *ble_get_df_driver();

void ble_on_received_data(const uint8_t *data, size_t length);
void ble_on_transmit_ready();
#endif /* BLE_DF_DRIVER_H */
