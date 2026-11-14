#include "settings.h"

#include "nrf_error.h"
#include "nrf_log.h"

#include "ble_amiibolink.h"
#include "tag_helper.h"
#include "vfs.h"
#include "vfs_meta.h"

#define SETTINGS_FILE_NAME "/settings.bin"

// Though OLED doesn't necessarily imply rechargeable battery, it's usually the case.
#define DEFAULT_BAT_MODE 1

const settings_data_t def_settings_data = {
    .auto_gen_amiibo = 0,
    .sleep_timeout_sec = 30,
    .bat_mode = DEFAULT_BAT_MODE,
    .amiibo_link_ver = BLE_AMIIBOLINK_VER_V1,
    .hibernate_enabled = true,
    .oled_contrast = 100,
    .anim_enabled = true,
    .amiidb_data_slot_num = 20,
    .qrcode_enabled = true,
    .chameleon_default_slot_index = INVALID_SLOT_INDEX,
    .app_enable_bits = 0xFFFF,
    .amiidb_sort_column = AMIIDB_SORT_COLUMN_NAME,
    .chameleon_slot_num = 8,
    .amiibolink_mode = 0, // 0 = not set, use default (manual)
};

settings_data_t m_settings_data = {0};

static bool m_settings_unread = false;

#define BOOL_VALIDATE(expr, default_val)                                                                               \
    if ((expr) != 0 && (expr) != 1)                                                                                    \
    {                                                                                                                  \
        (expr) = (default_val);                                                                                        \
    }
#define INT8_VALIDATE(expr, min, max, default_val)                                                                     \
    if ((expr) < (min) || (expr) > (max))                                                                              \
    {                                                                                                                  \
        (expr) = (default_val);                                                                                        \
    }

static void validate_settings()
{
    if (m_settings_data.sleep_timeout_sec > 180)
    {
        m_settings_data.sleep_timeout_sec = 30;
    }

    if (m_settings_data.amiibo_link_ver != BLE_AMIIBOLINK_VER_V1 &&
        m_settings_data.amiibo_link_ver != BLE_AMIIBOLINK_VER_V2 && m_settings_data.amiibo_link_ver != BLE_AMILOOP)
    {
        m_settings_data.amiibo_link_ver = BLE_AMIIBOLINK_VER_V1;
    }

    BOOL_VALIDATE(m_settings_data.hibernate_enabled, 0);
    BOOL_VALIDATE(m_settings_data.bat_mode, 0);
    BOOL_VALIDATE(m_settings_data.auto_gen_amiibo, 0);
    INT8_VALIDATE(m_settings_data.oled_contrast, 0, 100, 80);
    BOOL_VALIDATE(m_settings_data.anim_enabled, 0);
    BOOL_VALIDATE(m_settings_data.qrcode_enabled, 0);
    INT8_VALIDATE(m_settings_data.amiidb_data_slot_num, 1, 100, 20);
    INT8_VALIDATE(m_settings_data.amiidb_sort_column, AMIIDB_SORT_COLUMN_NAME, AMIIDB_SORT_COLUMN_POPULARITY,
                  AMIIDB_SORT_COLUMN_NAME);
    INT8_VALIDATE(m_settings_data.chameleon_slot_num, 8, 50, 8);
    INT8_VALIDATE(m_settings_data.chameleon_default_slot_index, 0, m_settings_data.chameleon_slot_num,
                  INVALID_SLOT_INDEX);

    // Validate amiibolink_mode: 0 = not set, 1-4 are valid modes
    if (m_settings_data.amiibolink_mode != 0 && m_settings_data.amiibolink_mode != BLE_AMIIBOLINK_MODE_RANDOM &&
        m_settings_data.amiibolink_mode != BLE_AMIIBOLINK_MODE_CYCLE &&
        m_settings_data.amiibolink_mode != BLE_AMIIBOLINK_MODE_NTAG &&
        m_settings_data.amiibolink_mode != BLE_AMIIBOLINK_MODE_RANDOM_AUTO_GEN)
    {
        m_settings_data.amiibolink_mode = 0; // Reset to "not set" if invalid
    }
}

int32_t settings_init()
{
    memcpy(&m_settings_data, &def_settings_data, sizeof(settings_data_t));
    vfs_driver_t *p_driver = vfs_get_default_driver();
    if (p_driver == NULL)
    {
        return NRF_ERROR_NOT_SUPPORTED;
    }
    int32_t err = 0;
    if (!p_driver->mounted())
    {
        err = p_driver->mount();
    }

    if (!p_driver->mounted())
    {
        m_settings_unread = true;
        return NRF_ERROR_INVALID_STATE;
    }

    uint8_t file_data[sizeof(settings_data_t) + 1];
    err = p_driver->read_file_data(SETTINGS_FILE_NAME, file_data, sizeof(file_data));
    if (err < 0)
    {
        m_settings_unread = err != VFS_ERR_NOOBJ;
        return NRF_ERROR_INVALID_STATE;
    }
    if (err != sizeof(settings_data_t))
    {
        return NRF_SUCCESS;
    }

    memcpy(&m_settings_data, file_data, sizeof(settings_data_t));
    validate_settings();

    NRF_LOG_INFO("settings loaded!");
    return NRF_SUCCESS;
}

int32_t settings_save()
{
    vfs_driver_t *p_driver = vfs_get_default_driver();
    int32_t err;

    if (p_driver == NULL)
    {
        return NRF_ERROR_NOT_SUPPORTED;
    }

    if (m_settings_unread && memcmp(&m_settings_data, &def_settings_data, sizeof(settings_data_t)) == 0)
    {
        return NRF_SUCCESS;
    }

    settings_data_t old_settings_data;
    bool needs_write = false;
    bool needs_meta_update = false;

    err = p_driver->read_file_data(SETTINGS_FILE_NAME, &old_settings_data, sizeof(settings_data_t));
    if (err == VFS_ERR_NOOBJ)
    {
        needs_write = true;
        needs_meta_update = true;
    }
    else if (err < 0)
    {
        NRF_LOG_WARNING("settings read failed (%d), recreating settings file", err);
        needs_write = true;
        needs_meta_update = true;
    }
    else if ((size_t) err != sizeof(settings_data_t))
    {
        NRF_LOG_WARNING("settings size mismatch (%d != %d), recreating settings file", err,
                        (int) sizeof(settings_data_t));
        needs_write = true;
        needs_meta_update = true;
    }
    else if (memcmp(&m_settings_data, &old_settings_data, sizeof(settings_data_t)) != 0)
    {
        needs_write = true;
    }

    if (needs_write)
    {
        err = p_driver->write_file_data(SETTINGS_FILE_NAME, &m_settings_data, sizeof(settings_data_t));
        if (err < 0)
        {
            return NRF_ERROR_INVALID_STATE;
        }
        m_settings_unread = false;

        if (needs_meta_update)
        {
            vfs_meta_t meta;
            memset(&meta, 0, sizeof(meta));
            meta.has_flags = true;
            meta.flags = VFS_OBJ_FLAG_HIDDEN;

            uint8_t meta_data[VFS_MAX_META_LEN];
            vfs_meta_encode(meta_data, sizeof(meta_data), &meta);
            err = p_driver->update_file_meta(SETTINGS_FILE_NAME, &meta_data, sizeof(meta_data));
            NRF_LOG_INFO("Settings file meta updated!: %d", err);
        }

        NRF_LOG_INFO("settings saved!");
    }

    return NRF_SUCCESS;
}

settings_data_t *settings_get_data()
{
    return &m_settings_data;
}

int32_t settings_reset()
{
    memcpy(&m_settings_data, &def_settings_data, sizeof(settings_data_t));
    m_settings_unread = false;
    vfs_driver_t *p_driver = vfs_get_default_driver();
    return p_driver->remove_file(SETTINGS_FILE_NAME);
}
