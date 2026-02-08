#include "py/mpconfig.h"
#include "py/runtime.h"
#include "extmod/modbluetooth.h"

#include "nvs_flash.h"
#include "nvs.h"

#define MP_BT_NVS_NAMESPACE "mp_bt"
// Big enough for ble_store_value_sec / ble_store_value_cccd blobs.
#define MP_BT_SECRET_MAX_LEN 128

STATIC nvs_handle_t mp_bt_nvs_handle;
STATIC bool mp_bt_nvs_inited = false;
STATIC uint8_t mp_bt_secret_buf[MP_BT_SECRET_MAX_LEN];

STATIC void mp_bt_nvs_ensure_init(void) {
    if (mp_bt_nvs_inited) {
        return;
    }
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err == ESP_OK && nvs_open(MP_BT_NVS_NAMESPACE, NVS_READWRITE, &mp_bt_nvs_handle) == ESP_OK) {
        mp_bt_nvs_inited = true;
    }
}

STATIC void mp_bt_make_key(char *out, size_t out_len,
                           uint8_t type, uint8_t index,
                           const uint8_t *key, size_t key_len) {
    switch (type) {
        case SECRET_TYPE_OUR_IRK:
            snprintf(out, out_len, "irk_our");
            break;
        case SECRET_TYPE_PEER_IRK:
            snprintf(out, out_len, "irk_peer_%u", index);
            break;
        case SECRET_TYPE_LTK:
            snprintf(out, out_len, "ltk_%u", index);
            break;
        case SECRET_TYPE_CSRK:
            snprintf(out, out_len, "csrk_%u", index);
            break;
        case SECRET_TYPE_CCCD:
            snprintf(out, out_len, "cccd_%u", index);
            break;
        default:
            snprintf(out, out_len, "t%u_%u", type, index);
            break;
    }
}

/*--------------------------------------------------------------------------*/
/*  New low-level, indexed secret API (for NimBLE store glue)               */
/*--------------------------------------------------------------------------*/

bool mp_bt_secret_get(uint8_t type, uint8_t index,
                      const uint8_t *key, size_t key_len,
                      const uint8_t **value, size_t *value_len) {
    mp_bt_nvs_ensure_init();
    if (!mp_bt_nvs_inited) {
        return false;
    }

    char nvs_key[16];
    mp_bt_make_key(nvs_key, sizeof(nvs_key), type, index, key, key_len);

    size_t len = sizeof(mp_bt_secret_buf);
    if (nvs_get_blob(mp_bt_nvs_handle, nvs_key, mp_bt_secret_buf, &len) != ESP_OK) {
        return false;
    }

    *value = mp_bt_secret_buf;
    *value_len = len;
    return true;
}

bool mp_bt_secret_set(uint8_t type, uint8_t index,
                      const uint8_t *key, size_t key_len,
                      const uint8_t *value, size_t value_len) {
    mp_bt_nvs_ensure_init();
    if (!mp_bt_nvs_inited) {
        return false;
    }

    char nvs_key[16];
    mp_bt_make_key(nvs_key, sizeof(nvs_key), type, index, key, key_len);

    if (nvs_set_blob(mp_bt_nvs_handle, nvs_key, value, value_len) != ESP_OK) {
        return false;
    }
    if (nvs_commit(mp_bt_nvs_handle) != ESP_OK) {
        return false;
    }
    return true;
}

bool mp_bt_secret_delete(uint8_t type, uint8_t index,
                         const uint8_t *key, size_t key_len) {
    mp_bt_nvs_ensure_init();
    if (!mp_bt_nvs_inited) {
        return false;
    }

    char nvs_key[16];
    mp_bt_make_key(nvs_key, sizeof(nvs_key), type, index, key, key_len);

    esp_err_t err = nvs_erase_key(mp_bt_nvs_handle, nvs_key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err != ESP_OK) {
        return false;
    }
    if (nvs_commit(mp_bt_nvs_handle) != ESP_OK) {
        return false;
    }
    return true;
}

/*--------------------------------------------------------------------------*/
/*  Existing MicroPython GAP secret API (kept for compatibility)            */
/*--------------------------------------------------------------------------*/

bool mp_bluetooth_gap_on_get_secret(uint8_t type, uint8_t index,
                                    const uint8_t *key, size_t key_len,
                                    const uint8_t **value, size_t *value_len) {
    // Existing callers use index=0; we just forward to the indexed API.
    return mp_bt_secret_get(type, index, key, key_len, value, value_len);
}

bool mp_bluetooth_gap_on_set_secret(uint8_t type,
                                    const uint8_t *key, size_t key_len,
                                    const uint8_t *value, size_t value_len) {
    // Existing callers don’t pass an index; treat as index 0.
    return mp_bt_secret_set(type, 0, key, key_len, value, value_len);
}
