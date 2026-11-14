#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_amiibolink.h"
#include "i18n/language.h"

#define INVALID_SLOT_INDEX            0xFF
#define AMIIDB_SORT_COLUMN_NAME       0
#define AMIIDB_SORT_COLUMN_POPULARITY 1

typedef struct
{
    uint8_t sleep_timeout_sec;
    bool dirty;
    bool auto_gen_amiibo;
    bool bat_mode;
    ble_amiibolink_ver_t amiibo_link_ver;
    bool hibernate_enabled;
    uint8_t oled_contrast;
    bool anim_enabled;
    bool qrcode_enabled;
    uint8_t amiidb_data_slot_num;
    uint8_t chameleon_default_slot_index;
    uint16_t app_enable_bits; // bitmask for app enable status (1: enabled, 0: disabled)
    uint8_t amiidb_sort_column;
    uint8_t chameleon_slot_num;            // chameleon available slot count (8-50)
    ble_amiibolink_mode_t amiibolink_mode; // user's preferred AmiiboLink mode (0 = not set, use default)
} settings_data_t;

int32_t settings_init();
int32_t settings_save();
int32_t settings_reset();
settings_data_t *settings_get_data();

#endif /* SETTINGS_H */
