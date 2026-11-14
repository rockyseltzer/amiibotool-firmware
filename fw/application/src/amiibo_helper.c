#include "amiibo_helper.h"

#include <string.h>

#include "nfc3d/amiibo.h"
#include "nrf_log.h"

#include "db_header.h"
#include "ntag_store.h"
#include "utils2.h"
#include "vfs.h"

#define UUID_OFFSET     468
#define AMII_ID_OFFSET  476
#define AMII_ID_OFFSET2 0x54
#define PASSWORD_OFFSET 532
#define PASSWORD_SIZE   4
#define UUID_SIZE       7

#define INFO_WRITES_OFFSET    0x29
#define INFO_FLAGS_OFFSET     0x2C
#define INFO_CREATED_OFFSET   0x30
#define INFO_MODIFIED_OFFSET  0x32
#define INFO_NICKNAME_OFFSET  0x38
#define INFO_OWNER_OFFSET     0x66
#define INFO_TITLE_ID_OFFSET  0xAC
#define INFO_APP_ID_OFFSET    0xB6
#define INFO_RESET_END_OFFSET 0x1B4
#define INFO_NAME_UNITS       10
#define INFO_FLAG_USER_DATA   0x10
#define INFO_FLAG_APP_DATA    0x20

// to avoid stack-allocating the new larger ntag_t struct
// static ntag_t ntag_new;

static nfc3d_amiibo_keys amiibo_keys;
static bool amiibo_keys_loaded;

// plain amiibo data static code
static const uint8_t Internal_StaticLock[8] = {0x65, 0x48, 0x0F, 0xE0, 0xF1, 0x10, 0xFF, 0xEE}; // 0x0
static const uint8_t V3_InternalLock[8] = {0x44, 0x00, 0x0F, 0xE0, 0xF1, 0x10, 0xFF, 0xEE};
static const uint8_t V3_ModelPad[4] = {0xAA, 0xBB, 0xCC, 0xDD};
static const uint8_t A5Static[4] = {0xA5, 0x00, 0x00, 0x00}; // 0x28
static const uint8_t DynLock[4] = {0x01, 0x00, 0x0F, 0xBD};  // 0x208
static const uint8_t Cfg0[4] = {0x00, 0x00, 0x00, 0x04};     // 0x20C
static const uint8_t Cfg1[4] = {0x5F, 0x00, 0x00, 0x00};     // 0x210

static const uint8_t LOCK[20] = {0x01, 0x00, 0x0f, 0xbd, 0x00, 0x00, 0x00, 0x04, 0x5f, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static void amiibo_helper_get_uuid(ntag_t *ntag, uint8_t *uid1)
{
    ntag_store_get_uuid(ntag, uid1);
}

// operates on internal data layout, but check bytes differ
static void amiibo_helper_replace_uuid(uint8_t *buffer, const uint8_t uuid[], bool tag_v3)
{
    if (tag_v3)
    {
        // writes to ntag215-data-1 section
        for (int i = 0; i < 7; i++)
        {
            buffer[UUID_OFFSET + i] = uuid[i];
        }
        buffer[UUID_OFFSET + 7] = 0x00; // rfu
    }
    else
    {
        uint8_t bcc[2];
        bcc[0] = 0x88 ^ uuid[0] ^ uuid[1] ^ uuid[2];
        bcc[1] = uuid[3] ^ uuid[4] ^ uuid[5] ^ uuid[6];

        int i;
        for (i = 0; i < 3; i++)
        {
            buffer[UUID_OFFSET + i] = uuid[i];
        }

        buffer[UUID_OFFSET + i] = bcc[0];
        i++;

        for (; i < 8; i++)
        {
            buffer[UUID_OFFSET + i] = uuid[i - 1];
        }

        // first byte of ntag215-data-2 section = byte 9 of uid (bcc1)
        buffer[0] = bcc[1];
    }
}

// operates on internal data layout
static void amiibo_helper_replace_password(uint8_t *buffer, const uint8_t uuid[])
{
    uint8_t password[PASSWORD_SIZE] = {0, 0, 0, 0};

    password[0] = 0xAA ^ uuid[1] ^ uuid[3];
    password[1] = 0x55 ^ uuid[2] ^ uuid[4];
    password[2] = 0xAA ^ uuid[3] ^ uuid[5];
    password[3] = 0x55 ^ uuid[4] ^ uuid[6];

    for (int i = 0; i < PASSWORD_SIZE; i++)
    {
        buffer[PASSWORD_OFFSET + i] = password[i];
    }
}

// 使用uuid生成amiibo数据
// operates on internal data layout
static void amiibo_helper_set_defaults(uint8_t *buffer, const uint8_t uuid[], bool tag_v3)
{
    // set keygen salt
    ret_code_t err_code = utils_rand_bytes(&buffer[0x1E8], 32);
    APP_ERROR_CHECK(err_code);

    // ntag data 2
    if (!tag_v3)
    {
        memcpy(buffer, Internal_StaticLock, 8);
        // set BCC
        buffer[0] = uuid[3] ^ uuid[4] ^ uuid[5] ^ uuid[6];
    }

    amiibo_helper_replace_uuid(buffer, uuid, tag_v3);
    amiibo_helper_replace_password(buffer, uuid);

    memcpy(&buffer[0x28], A5Static, 4);

    // note: these sections are never written back when re-packing
    memcpy(&buffer[0x208], DynLock, 4);
    memcpy(&buffer[0x20C], Cfg0, 4);
    memcpy(&buffer[0x210], Cfg1, 4);
    // auth magic value
    buffer[0x218] = 0;
    buffer[0x219] = 0;
}

ret_code_t amiibo_helper_load_keys(const uint8_t *data)
{
    if (!nfc3d_amiibo_load_keys(&amiibo_keys, data))
    {
        return NRF_ERROR_INVALID_DATA;
    }
    amiibo_keys_loaded = true;
    return NRF_SUCCESS;
}

bool amiibo_helper_is_key_loaded()
{
    return amiibo_keys_loaded;
}

static ret_code_t amiibo_helper_resign_with_uuid(ntag_t *ntag, const uint8_t uuid[])
{
    if (!is_valid_amiibo_ntag(ntag) || !amiibo_helper_is_key_loaded())
    {
        return NRF_ERROR_INVALID_DATA;
    }

    uint8_t plain[NTAG215_SIZE];
    bool tag_v3 = ntag->type == NTAG_I2C_PLUS_2K;
    if (!nfc3d_amiibo_unpack(&amiibo_keys, ntag->data, plain, tag_v3))
    {
        return NRF_ERROR_INVALID_DATA;
    }

    amiibo_helper_replace_uuid(plain, uuid, tag_v3);
    amiibo_helper_set_defaults(plain, uuid, tag_v3);
    nfc3d_amiibo_pack(&amiibo_keys, plain, ntag->data, tag_v3);
    return NRF_SUCCESS;
}

static uint16_t amiibo_helper_read_u16_be(const uint8_t *data)
{
    return (uint16_t) (data[0] << 8 | data[1]);
}

static void amiibo_helper_utf16_to_utf8(const uint8_t *src, size_t units, bool big_endian, char *dst, size_t dst_size)
{
    size_t out = 0;
    for (size_t i = 0; i < units; i++)
    {
        const uint8_t *unit = &src[i * 2];
        uint16_t c = big_endian ? amiibo_helper_read_u16_be(unit) : (uint16_t) (unit[1] << 8 | unit[0]);
        if (c == 0)
        {
            break;
        }
        if (c >= 0xD800 && c <= 0xDFFF)
        {
            c = '?';
        }
        size_t need = c < 0x80 ? 1 : (c < 0x800 ? 2 : 3);
        if (out + need >= dst_size)
        {
            break;
        }
        if (need == 1)
        {
            dst[out] = (char) c;
            out++;
        }
        else if (need == 2)
        {
            dst[out] = (char) (0xC0 | c >> 6);
            dst[out + 1] = (char) (0x80 | (c & 0x3F));
            out += 2;
        }
        else
        {
            dst[out] = (char) (0xE0 | c >> 12);
            dst[out + 1] = (char) (0x80 | (c >> 6 & 0x3F));
            dst[out + 2] = (char) (0x80 | (c & 0x3F));
            out += 3;
        }
    }
    dst[out] = '\0';
}

ret_code_t amiibo_helper_read_info(const ntag_t *ntag, amiibo_info_t *p_info)
{
    if (!amiibo_helper_is_key_loaded())
    {
        return NRF_ERROR_INVALID_STATE;
    }

    uint8_t plain[NTAG215_SIZE];
    bool tag_v3 = ntag->type == NTAG_I2C_PLUS_2K;
    if (!nfc3d_amiibo_unpack(&amiibo_keys, ntag->data, plain, tag_v3))
    {
        return NRF_ERROR_INVALID_DATA;
    }

    memset(p_info, 0, sizeof(amiibo_info_t));
    p_info->writes = amiibo_helper_read_u16_be(&plain[INFO_WRITES_OFFSET]);
    p_info->has_user_data = (plain[INFO_FLAGS_OFFSET] & INFO_FLAG_USER_DATA) != 0;
    p_info->has_app_data = (plain[INFO_FLAGS_OFFSET] & INFO_FLAG_APP_DATA) != 0;

    if (p_info->has_user_data)
    {
        p_info->created = amiibo_helper_read_u16_be(&plain[INFO_CREATED_OFFSET]);
        p_info->modified = amiibo_helper_read_u16_be(&plain[INFO_MODIFIED_OFFSET]);
        amiibo_helper_utf16_to_utf8(&plain[INFO_NICKNAME_OFFSET], INFO_NAME_UNITS, true, p_info->nickname,
                                    sizeof(p_info->nickname));
        amiibo_helper_utf16_to_utf8(&plain[INFO_OWNER_OFFSET], INFO_NAME_UNITS, false, p_info->owner,
                                    sizeof(p_info->owner));
    }
    if (p_info->has_app_data)
    {
        p_info->title_id_high = to_little_endian_int32(&plain[INFO_TITLE_ID_OFFSET]);
        p_info->app_id = to_little_endian_int32(&plain[INFO_APP_ID_OFFSET]);
    }
    return NRF_SUCCESS;
}

ret_code_t amiibo_helper_reset_amiibo(ntag_t *ntag)
{
    if (!is_valid_amiibo_ntag(ntag) || !amiibo_helper_is_key_loaded())
    {
        return NRF_ERROR_INVALID_STATE;
    }

    uint8_t plain[NTAG215_SIZE];
    bool tag_v3 = ntag->type == NTAG_I2C_PLUS_2K;
    if (!nfc3d_amiibo_unpack(&amiibo_keys, ntag->data, plain, tag_v3))
    {
        return NRF_ERROR_INVALID_DATA;
    }

    memset(&plain[INFO_WRITES_OFFSET], 0, INFO_RESET_END_OFFSET - INFO_WRITES_OFFSET);

    nfc3d_amiibo_pack(&amiibo_keys, plain, ntag->data, tag_v3);
    return NRF_SUCCESS;
}

bool amiibo_helper_format_date(uint16_t date, char *buff, size_t buff_size)
{
    uint16_t month = date >> 5 & 0x0F;
    uint16_t day = date & 0x1F;
    if (month < 1 || month > 12 || day < 1)
    {
        return false;
    }
    snprintf(buff, buff_size, "%04u-%02u-%02u", 2000 + (date >> 9), month, day);
    return true;
}

ret_code_t amiibo_helper_rand_amiibo_uuid(ntag_t *ntag)
{
    uint8_t uuid[UUID_SIZE];
    uuid[0] = 0x04;
    ret_code_t err_code = utils_rand_bytes(&uuid[1], UUID_SIZE - 1);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    return amiibo_helper_resign_with_uuid(ntag, uuid);
}

ret_code_t amiibo_helper_set_amiibo_uuid(ntag_t *ntag, uint8_t *uuid)
{
    return amiibo_helper_resign_with_uuid(ntag, uuid);
}

static ret_code_t amiibo_helper_generate_amiibo_ntag215(uint32_t head, uint32_t tail, ntag_t *ntag)
{
    if (!amiibo_helper_is_key_loaded())
    {
        return NRF_ERROR_INVALID_DATA;
    }

    ntag->type = NTAG_215;

    // 随机uuid
    uint8_t uuid[UUID_SIZE];
    ntag_store_new_rand(ntag);
    amiibo_helper_get_uuid(ntag, uuid);

    uint8_t modified[NTAG215_SIZE];
    memset(modified, 0, NTAG215_SIZE);
    // 填充amiibo id
    int32_to_bytes_le(head, &modified[AMII_ID_OFFSET]);
    int32_to_bytes_le(tail, &modified[AMII_ID_OFFSET + 4]);
    int32_to_bytes_le(head, &modified[AMII_ID_OFFSET2]);
    int32_to_bytes_le(tail, &modified[AMII_ID_OFFSET2 + 4]);
    // 填充特定数据
    amiibo_helper_set_defaults(modified, uuid, false);

    // encrypt
    nfc3d_amiibo_pack(&amiibo_keys, modified, ntag->data, false);

    // 最后20位字节，非常关键
    memcpy(&ntag->data[520], LOCK, sizeof(LOCK));

    return NRF_SUCCESS;
}

static ret_code_t amiibo_helper_generate_amiibo_v3(uint32_t head, uint32_t tail, ntag_t *ntag)
{
    if (!amiibo_helper_is_key_loaded())
    {
        return NRF_ERROR_INVALID_DATA;
    }

    uint8_t uuid[UUID_SIZE];
    ntag_store_new_rand(ntag);
    amiibo_helper_get_uuid(ntag, uuid);

    ntag->type = NTAG_I2C_PLUS_2K;
    memset(ntag->data, 0, sizeof(ntag->data));

    uint8_t modified[NTAG215_SIZE];
    memset(modified, 0, NTAG215_SIZE);
    int32_to_bytes_le(head, &modified[AMII_ID_OFFSET]);
    int32_to_bytes_le(tail, &modified[AMII_ID_OFFSET + 4]);
    int32_to_bytes_le(head, &modified[AMII_ID_OFFSET2]);
    int32_to_bytes_le(tail, &modified[AMII_ID_OFFSET2 + 4]);
    memcpy(modified, V3_InternalLock, sizeof(V3_InternalLock));
    memcpy(&modified[AMII_ID_OFFSET + 8], V3_ModelPad, sizeof(V3_ModelPad));
    amiibo_helper_set_defaults(modified, uuid, true);

    // encrypt
    nfc3d_amiibo_pack(&amiibo_keys, modified, ntag->data, true);

    memcpy(&ntag->data[0x388], (uint8_t[]) {0x01, 0x00, 0xFF, 0x00}, 4); // Page E2: dynamic lock bytes
    memcpy(&ntag->data[0x38C], (uint8_t[]) {0x00, 0x00, 0x00, 0x04}, 4); // Page E3: auth0
    memcpy(&ntag->data[0x390], (uint8_t[]) {0x07, 0x00, 0x00, 0x00}, 4); // Page E4: access
    memcpy(&ntag->data[0x39C], (uint8_t[]) {0x08, 0x00, 0x00, 0x00}, 4);
    memcpy(&ntag->data[0x3A0], (uint8_t[]) {0x01, 0x00, 0xF8, 0x48}, 4);
    memcpy(&ntag->data[0x3A4], (uint8_t[]) {0x08, 0x01, 0x00, 0x00}, 4);
    memcpy(&ntag->data[0x3B0], (uint8_t[]) {0x41, 0x00, 0xF8, 0x48}, 4);
    memcpy(&ntag->data[0x3B4], (uint8_t[]) {0x08, 0x01, 0x21, 0x00}, 4);

    size_t sram_data_offset = 0x3C0;
    uint16_t crc = crc16_mcrf4xx(&ntag->data[sram_data_offset], 0x3e);
    ntag->data[sram_data_offset + 0x3e] = (crc >> 8) & 0xFF;
    ntag->data[sram_data_offset + 0x3f] = crc & 0xFF;

    return NRF_SUCCESS;
}

ret_code_t amiibo_helper_generate_amiibo(uint32_t head, uint32_t tail, ntag_t *ntag)
{
    if (is_valid_amiibo_v3(head, tail))
    {
        return amiibo_helper_generate_amiibo_v3(head, tail, ntag);
    }
    else
    {
        return amiibo_helper_generate_amiibo_ntag215(head, tail, ntag);
    }
}

void amiibo_helper_try_load_amiibo_keys_from_vfs()
{
    if (!amiibo_helper_is_key_loaded() && vfs_drive_enabled(VFS_DRIVE_EXT))
    {
        uint8_t key_data[160];
        vfs_driver_t *p_driver = vfs_get_driver(VFS_DRIVE_EXT);
        int32_t err = p_driver->read_file_data("/key_retail.bin", key_data, sizeof(key_data));
        NRF_LOG_INFO("amiibo key read: %d", err);
        if (err == sizeof(key_data))
        {
            ret_code_t ret = amiibo_helper_load_keys(key_data);
            if (ret == NRF_SUCCESS)
            {
                NRF_LOG_INFO("amiibo key loaded!");
            }
        }
    }
}

uint32_t to_little_endian_int32(const uint8_t *data)
{
    uint32_t val = 0;
    val += data[0];
    val <<= 8;
    val += data[1];
    val <<= 8;
    val += data[2];
    val <<= 8;
    val += data[3];
    return val;
}

bool is_valid_amiibo_ntag(const ntag_t *ntag)
{
    uint32_t head = to_little_endian_int32(&ntag->data[84]);
    uint32_t tail = to_little_endian_int32(&ntag->data[88]);

    if (ntag->data[0] != 0x4)
    {
        return false;
    }

    const db_amiibo_t *amd = get_amiibo_by_id(head, tail);
    if (amd != NULL)
    {
        return true;
    }
    if (head != 0 && tail != 0)
    {
        return true;
    }

    return false;
}

uint16_t crc16_mcrf4xx(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0x8408;
            }
            else
            {
                crc = (crc >> 1);
            }
        }
    }
    return crc & 0xffff;
}
