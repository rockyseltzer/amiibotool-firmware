/*
 * t2t_emu.c
 *
 *  Created on: 2021年5月30日
 *      Author: solos
 */

#include "app_scheduler.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "boards.h"
#include "nfc_platform.h"
#include "nrf52.h"
#include "nrf52_bitfields.h"
#include "nrf_log.h"
#include "nrf_pwr_mgmt.h"
#include "nrfx.h"

#include "cache.h"
#include "hal_nfc_t2t.h"
#include "ntag_emu.h"

typedef struct
{
    ntag_t ntag;
    uint8_t dirty;
    uint8_t busy;
    int sector;
    bool is_selecting_sector;
    ntag_update_cb_t update_cb;
    void *cb_context;
} ntag_emu_t;

static ntag_emu_t ntag_emu = {0};

static const uint8_t NTAG215_Version[8] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
static const uint8_t NTAGI2CPLUS2K_Version[8] = {0x00, 0x04, 0x04, 0x05, 0x02, 0x02, 0x15, 0x03};
// NTAG215_Version[7] mean:
// 0x0F ntag213
// 0x11 ntag215
// 0x13 ntag216

static const uint8_t NTAG215_Signature[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t NTAG215_PwdOK[2] = {
    0x80,
    0x80,
};

static uint8_t N2E_INFO[4] = {0x00, 0x32, 0x00, 0x03};

static const uint8_t N2E_ID[16] = {
    0x21, 0x4b, 0x87, 0x02, 0x52, 0x3d, 0x0d, 0x10, 0x16, 0x3a, 0x24, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const uint8_t N2E_SELECT_BANK[1] = {0x0a};

// NTAG COMMANDS
#define NFC_CMD_READ          0x30
#define NFC_CMD_WRITE         0xA2
#define NFC_CMD_GET_VERSION   0x60
#define NFC_CMD_READ_SIG      0x3C
#define NFC_CMD_PWD_AUTH      0x1B
#define NFC_CMD_FAST_READ     0x3A
#define NFC_CMD_FAST_WRITE    0xA6
#define NFC_CMD_SECTOR_SELECT 0xC2

// N2ELITE COMMANDS
#define N2_CMD_GET_INFO    0x55
#define N2_CMD_GET_ID      0x43
#define N2_CMD_FAST_READ   0x3B
#define N2_CMD_WRITE       0xA5
#define N2_CMD_SELECT_BANK 0xA7
#define N2_CMD_FAST_WRITE  0xAE
// see https://wiki.yobi.be/index.php/N2_Elite

#define NTAG_I2C_PAGE_NS_REG 0xED
#define NS_REG_SRAM_RF_READY 0b1000

static void update_ntag_handler(void *p_event_data, uint16_t event_size);

static void nfc_received_process(const uint8_t *p_data, size_t data_length, uint8_t *plain)
{
    const uint8_t *data = p_data;
    size_t length = data_length;

    nrf_pwr_mgmt_feed();

    if (length == 0)
    {
        hal_send_ack_nack(0x0);
        return;
    }

    if (ntag_emu.is_selecting_sector)
    {
        ntag_emu.is_selecting_sector = false;
        ntag_emu.sector = data[0];
        NRF_LOG_INFO("NFC Sector Select Done (switch to %d)", data[0]);

        // To successfully access to the requested memory sector, the tag shall issue a passive ACK, which is sending NO
        // REPLY for more than 1 ms after the CRC of the second command set.
        APP_ERROR_CHECK(hal_nfc_passive_ack());
        return;
    }

    uint8_t command = data[0];
    uint8_t block_num = (length > 1) ? data[1] : 0;
    size_t ntag_data_size = _ntag_data_size(&ntag_emu.ntag);

    switch (command)
    {
        case NFC_CMD_READ:
            if (length < 2)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("NFC Read Block %d", block_num);
            int full_block = ((int) ntag_emu.sector * 256 + block_num);
            size_t read_offset = (size_t) full_block * 4u;
            if ((full_block >= 0) && (read_offset <= (ntag_data_size - 16u)))
            {
                static uint8_t chunk_data[16] = {0};
                memcpy(chunk_data, &plain[read_offset], 16);

                // READ command will return 4 pages (16 bytes)
                // empirically, S2 with JC1 will poll page 0xEC to read SRAM_RF_READY (which is in ED)
                if (ntag_emu.ntag.type == NTAG_I2C_PLUS_2K && full_block <= NTAG_I2C_PAGE_NS_REG &&
                    full_block + 3 >= NTAG_I2C_PAGE_NS_REG)
                {
                    // set NS_REG.SRAM_RF_READY (byte 2 of page ED)
                    // switch 2 will poll until this is set, and *then* read sram pages
                    chunk_data[(NTAG_I2C_PAGE_NS_REG - full_block) * 4 + 2] |= NS_REG_SRAM_RF_READY;
                }

                APP_ERROR_CHECK(hal_nfc_send(chunk_data, 16));
            }
            else
            {
                hal_send_ack_nack(0x0);
            }
            break;
        case N2_CMD_WRITE:
            // similar to write but with one extra paramter to specify the bank number
            if (length < 7)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("N2E Write slot:%d:", data[2]);
            length = 6;
            data = &data[1];
            /* fallthrough */
        case NFC_CMD_WRITE:
            if (length != 6)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("NFC Write Block %d", block_num);
            if (ntag_emu.ntag.read_only)
            {
                NRF_LOG_INFO("NFC Read Only");
                hal_send_ack_nack(0x0);
                return;
            }
            int full_block_num = (((int) ntag_emu.sector) * 256) + block_num;
            size_t write_offset = (size_t) full_block_num * 4u;
            if ((full_block_num < 0) || (write_offset > (ntag_data_size - 4u)))
            {
                hal_send_ack_nack(0x0);
                break;
            }
            ntag_emu.dirty = true;
            if (ntag_emu.ntag.type == NTAG_215 && (full_block_num == 133 || full_block_num == 134))
            {
                // ignore
            }
            else if (full_block_num == 2)
            {
                plain[write_offset + 2] = data[4];
                plain[write_offset + 3] = data[5];
            }
            else
            {
                for (int i = 0; i < 4; i++)
                {
                    plain[write_offset + i] = data[i + 2];
                }
            }
            hal_send_ack_nack(0xA);
            break;
        case NFC_CMD_GET_VERSION:
            NRF_LOG_INFO("NFC Get Version for type %d", ntag_emu.ntag.type);
            if (ntag_emu.ntag.type == NTAG_I2C_PLUS_2K)
            {
                APP_ERROR_CHECK(hal_nfc_send(NTAGI2CPLUS2K_Version, 8));
            }
            else
            {
                APP_ERROR_CHECK(hal_nfc_send(NTAG215_Version, 8));
            }
            break;
        case NFC_CMD_READ_SIG:
            NRF_LOG_INFO("NFC Read Signature");
            hal_nfc_send(NTAG215_Signature, 32);
            break;
        case N2_CMD_FAST_READ: // TODO
            if (length < 4)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("N2E Fast Read slot %d:", data[3]);
            /* fallthrough */
        case NFC_CMD_FAST_READ:
            if (length < 3)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            int start_page = block_num + (ntag_emu.sector * 256);
            int end_page = data[2] + (ntag_emu.sector * 256);
            int page_count = end_page - start_page + 1;
            size_t fast_read_offset = (size_t) start_page * 4u;
            size_t fast_read_length = (size_t) page_count * 4u;

            if ((start_page < 0) || (end_page < start_page) || (page_count <= 0) || (fast_read_length > UINT8_MAX) ||
                (fast_read_offset > ntag_data_size) || (fast_read_length > (ntag_data_size - fast_read_offset)))
            {
                hal_send_ack_nack(0x0);
                break;
            }

            if (ntag_emu.ntag.type == NTAG_I2C_PLUS_2K && start_page <= NTAG_I2C_PAGE_NS_REG &&
                end_page >= NTAG_I2C_PAGE_NS_REG)
            {
                // set NS_REG.SRAM_RF_READY
                // switch 2 will poll until this is set, and *then* read sram pages
                uint8_t *ns_reg = &plain[NTAG_I2C_PAGE_NS_REG * 4 + 2];
                uint8_t ns_reg_stored = *ns_reg;
                *ns_reg |= NS_REG_SRAM_RF_READY;
                hal_nfc_send(&plain[fast_read_offset], fast_read_length);
                *ns_reg = ns_reg_stored;
            }
            else
            {
                hal_nfc_send(&plain[fast_read_offset], fast_read_length);
            }
            break;
        case NFC_CMD_PWD_AUTH:
            if (length < 5)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("NFC Password: %x %x %x %x", data[1], data[2], data[3], data[4]);
            hal_nfc_send(NTAG215_PwdOK, 2);
            break;
        case N2_CMD_GET_INFO:
            N2E_INFO[0] = 0;
            hal_nfc_send(N2E_INFO, 4);
            break;
        case N2_CMD_GET_ID:
            hal_nfc_send(N2E_ID, 16);
            break;
        case N2_CMD_SELECT_BANK:
            // uint8_t banknum = block_num;
            hal_nfc_send(N2E_SELECT_BANK, 1);
            break;
        case N2_CMD_FAST_WRITE:
            if (length < 4)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("N2E Fast Read slot %d:", data[2]);
            if (ntag_emu.ntag.read_only)
            {
                NRF_LOG_INFO("NFC Read Only");
                hal_send_ack_nack(0x0);
                break;
            }
            uint8_t datasize = data[3];
            size_t fast_write_offset = ((size_t) ntag_emu.sector * 256u + block_num) * 4u;

            if ((length < (size_t) datasize + 4u) || (fast_write_offset > ntag_data_size) ||
                ((size_t) datasize > (ntag_data_size - fast_write_offset)))
            {
                hal_send_ack_nack(0x0);
                break;
            }

            ntag_emu.dirty = true;
            for (int i = 0; i < datasize; i++)
            {
                size_t offset = fast_write_offset + i;
                if (offset == 8 || offset == 9 || (ntag_emu.ntag.type == NTAG_215 && offset >= 532 && offset < 540))
                {
                    continue;
                }
                plain[offset] = data[i + 4];
            }
            hal_send_ack_nack(0xA);
            break;
        case NFC_CMD_SECTOR_SELECT:
            NRF_LOG_INFO("NFC Sector Select start");
            hal_send_ack_nack(0xA);
            ntag_emu.is_selecting_sector = true;
            break;
        case NFC_CMD_FAST_WRITE:
            // only supported for page 0xf0-0xff in sector 0 to write to sram buffer on i2c
            // for now we ignore the writes and just ack
            if (length < 3)
            {
                hal_send_ack_nack(0x0);
                break;
            }
            NRF_LOG_INFO("NFC Fast Write %x to %x", block_num, data[2]);
            hal_send_ack_nack(0xA);
            break;
        default:
            NRF_LOG_INFO("NFC CMD %x", data[0]);
            hal_send_ack_nack(0x0);
            break;
    }
}

/**
 * @brief Callback function for handling NFC events.
 */

static void nfc_callback(void *p_context, hal_nfc_event_t event, const uint8_t *p_data, size_t data_length)
{
    (void) p_context;

    switch (event)
    {
        case HAL_NFC_EVENT_FIELD_ON:
            bsp_board_led_on(BSP_BOARD_LED_0);
            break;
        case HAL_NFC_EVENT_FIELD_OFF:
            bsp_board_led_off(BSP_BOARD_LED_0);
            ntag_emu.sector = 0;
            ntag_emu.is_selecting_sector = false;
            if (ntag_emu.dirty)
            {
                ntag_event_type_t type = NTAG_EVENT_TYPE_WRITTEN;
                app_sched_event_put(&type, sizeof(ntag_event_type_t), update_ntag_handler);
            }
            else
            {
                ntag_event_type_t type = NTAG_EVENT_TYPE_READ;
                app_sched_event_put(&type, sizeof(ntag_event_type_t), update_ntag_handler);
            }
            break;
        case HAL_NFC_EVENT_COMMAND:
            nfc_received_process(p_data, data_length, ntag_emu.ntag.data);
            break;
        case HAL_NFC_EVENT_DATA_TRANSMITTED:
            break;
        default:
            break;
    }
}

void ntag_emu_set_update_cb(ntag_update_cb_t cb, void *context)
{
    ntag_emu.update_cb = cb;
    ntag_emu.cb_context = context;
}

ntag_t *ntag_emu_get_current_tag()
{
    return &(ntag_emu.ntag);
}

static void update_ntag_handler(void *p_event_data, uint16_t event_size)
{
    ntag_t *p_snapshot = &cache_get_data()->ntag;

    ntag_emu.busy = true;

    ntag_event_type_t type = *((ntag_event_type_t *) p_event_data);

    CRITICAL_REGION_ENTER();
    memcpy(p_snapshot, &(ntag_emu.ntag), sizeof(ntag_t));
    ntag_emu.dirty = false;
    CRITICAL_REGION_EXIT();

    ntag_emu_set_uuid_only(p_snapshot);

    if (ntag_emu.update_cb)
    {
        ntag_emu.update_cb(type, ntag_emu.cb_context, p_snapshot);
    }

    ntag_emu.busy = false;
}

ret_code_t ntag_emu_init(const ntag_t *ntag)
{
    uint32_t err_code;

    err_code = hal_nfc_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = hal_nfc_start();
    APP_ERROR_CHECK(err_code);

    ntag_emu_set_tag(ntag);
    return NRF_SUCCESS;
}

void ntag_emu_set_tag(const ntag_t *ntag)
{
    CRITICAL_REGION_ENTER();
    memcpy(&(ntag_emu.ntag), ntag, sizeof(ntag_t));
    ntag_emu.dirty = false;
    ntag_emu.sector = 0;
    ntag_emu.is_selecting_sector = 0;
    CRITICAL_REGION_EXIT();

    ntag_emu_set_uuid_only(ntag);
}

void ntag_emu_set_uuid_only(const ntag_t *ntag)
{
    uint8_t uid1[7];
    if (ntag->type == NTAG_I2C_PLUS_2K)
    {
        // i really don't understand why this is different
        // but we need to present the id bytes directly here
        // *without* the bcc0/bcc1 bytes, these are NOT valid for this tag type
        uid1[0] = ntag->data[0];
        uid1[1] = ntag->data[1];
        uid1[2] = ntag->data[2];
        uid1[3] = ntag->data[3];
        uid1[4] = ntag->data[4];
        uid1[5] = ntag->data[5];
        uid1[6] = ntag->data[6];
    }
    else
    {
        uid1[0] = ntag->data[0];
        uid1[1] = ntag->data[1];
        uid1[2] = ntag->data[2];
        uid1[3] = ntag->data[4];
        uid1[4] = ntag->data[5];
        uid1[5] = ntag->data[6];
        uid1[6] = ntag->data[7];
    }

    hal_nfc_parameter_set(HAL_NFC_PARAM_ID_NFCID1, uid1, sizeof(uid1));
}
