#include "sx_wifi.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "sx_wifi";

/* NVS namespace/keys for saved STA credentials. Kept private to this
 * translation unit -- callers only ever see the sx_wifi_* API, never
 * the storage details, so this layout can change without breaking
 * anything above it. */
#define SX_WIFI_NVS_NAMESPACE  "sx_wifi"
#define SX_WIFI_NVS_KEY_SSID   "sta_ssid"
#define SX_WIFI_NVS_KEY_PASS   "sta_pass"

/* STA connection policy: how many times to retry esp_wifi_connect()
 * after a disconnect before giving up and reporting failure back to
 * the blocking caller. */
#define SX_WIFI_STA_MAX_RETRY  5

/* Bits used on the event group that sx_wifi_start_sta() blocks on. */
#define SX_WIFI_CONNECTED_BIT  BIT0
#define SX_WIFI_FAIL_BIT       BIT1

static bool s_core_initialized = false;      /* NVS + event loop + netif done */
static bool s_wifi_driver_initialized = false; /* esp_wifi_init() done */

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_sta_retry_count = 0;
static esp_event_handler_instance_t s_instance_any_id = NULL;
static esp_event_handler_instance_t s_instance_got_ip = NULL;

/* ---- core init (NVS / event loop / netif) --------------------------- */

bool sx_wifi_init(void)
{
    if (s_core_initialized) {
        return true;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition layout changed (or first-ever boot on this
         * partition table) -- erase and retry once, matching the
         * standard ESP-IDF Wi-Fi example pattern. */
        ESP_LOGW(TAG, "NVS needs erase (err=%s), reinitializing", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means a default loop already exists,
         * which is fine (e.g. re-entering sx_wifi_init() safely). */
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return false;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "xEventGroupCreate failed (out of memory?)");
        return false;
    }

    s_core_initialized = true;
    return true;
}

/* Lazily creates the esp_wifi driver instance itself (as opposed to
 * the NVS/netif/event-loop core above). Cheap to call more than once;
 * only does real work the first time. Needed before both STA and AP
 * start paths. */
static bool ensure_wifi_driver(void)
{
    if (s_wifi_driver_initialized) {
        return true;
    }
    if (!sx_wifi_init()) {
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }

    s_wifi_driver_initialized = true;
    return true;
}

/* ---- STA mode --------------------------------------------------------- */

static void sta_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_sta_retry_count < SX_WIFI_STA_MAX_RETRY) {
            esp_wifi_connect();
            s_sta_retry_count++;
            ESP_LOGI(TAG, "STA disconnected, retrying (%d/%d)",
                     s_sta_retry_count, SX_WIFI_STA_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, SX_WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, SX_WIFI_CONNECTED_BIT);
    }
}

bool sx_wifi_start_sta(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') {
        ESP_LOGE(TAG, "sx_wifi_start_sta: empty ssid");
        return false;
    }
    if (!ensure_wifi_driver()) {
        return false;
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "esp_netif_create_default_wifi_sta failed");
        return false;
    }

    xEventGroupClearBits(s_wifi_event_group, SX_WIFI_CONNECTED_BIT | SX_WIFI_FAIL_BIT);
    s_sta_retry_count = 0;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL, &s_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL, &s_instance_got_ip));

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password != NULL) {
        strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }
    wifi_config.sta.threshold.authmode = (password != NULL && password[0] != '\0')
                                              ? WIFI_AUTH_WPA2_PSK
                                              : WIFI_AUTH_OPEN;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    err = err == ESP_OK ? esp_wifi_set_config(WIFI_IF_STA, &wifi_config) : err;
    err = err == ESP_OK ? esp_wifi_start() : err;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STA setup failed: %s", esp_err_to_name(err));
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        return false;
    }

    ESP_LOGI(TAG, "Connecting to SSID '%s'...", ssid);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            SX_WIFI_CONNECTED_BIT | SX_WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);

    bool connected = (bits & SX_WIFI_CONNECTED_BIT) != 0;
    if (!connected) {
        ESP_LOGW(TAG, "Failed to connect to SSID '%s' after %d retries",
                 ssid, SX_WIFI_STA_MAX_RETRY);
        esp_wifi_stop();
    }

    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
    s_instance_any_id = NULL;
    s_instance_got_ip = NULL;

    return connected;
}

/* ---- AP mode ------------------------------------------------------- */

bool sx_wifi_start_ap(const char *ap_ssid, const char *ap_password)
{
    if (ap_ssid == NULL || ap_ssid[0] == '\0') {
        ESP_LOGE(TAG, "sx_wifi_start_ap: empty ssid");
        return false;
    }
    bool has_password = (ap_password != NULL && ap_password[0] != '\0');
    if (has_password && strlen(ap_password) < 8) {
        ESP_LOGE(TAG, "sx_wifi_start_ap: password must be >= 8 chars for WPA2-PSK");
        return false;
    }
    if (!ensure_wifi_driver()) {
        return false;
    }

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "esp_netif_create_default_wifi_ap failed");
        return false;
    }

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    wifi_config.ap.channel = SX_WIFI_DEFAULT_AP_CHANNEL;
    wifi_config.ap.max_connection = SX_WIFI_DEFAULT_AP_MAX_CONN;

    if (has_password) {
        strlcpy((char *)wifi_config.ap.password, ap_password, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    err = err == ESP_OK ? esp_wifi_set_config(WIFI_IF_AP, &wifi_config) : err;
    err = err == ESP_OK ? esp_wifi_start() : err;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP setup failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "SoftAP started: SSID '%s', %s, channel %d",
             ap_ssid, has_password ? "WPA2-PSK" : "open", SX_WIFI_DEFAULT_AP_CHANNEL);
    return true;
}

bool sx_wifi_start_default_ap(void)
{
    uint8_t mac[6] = { 0 };
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char ssid[SX_WIFI_SSID_MAX_LEN];

    if (err == ESP_OK) {
        snprintf(ssid, sizeof(ssid), "%s%02X%02X%02X",
                 SX_WIFI_DEFAULT_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGW(TAG, "esp_read_mac failed: %s, using fixed SSID suffix",
                 esp_err_to_name(err));
        snprintf(ssid, sizeof(ssid), "%s000000", SX_WIFI_DEFAULT_AP_SSID_PREFIX);
    }

    return sx_wifi_start_ap(ssid, NULL);
}

/* ---- credential storage --------------------------------------------- */

bool sx_wifi_save_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) >= SX_WIFI_SSID_MAX_LEN) {
        ESP_LOGE(TAG, "sx_wifi_save_credentials: invalid ssid");
        return false;
    }
    if (password != NULL && strlen(password) >= SX_WIFI_PASSWORD_MAX_LEN) {
        ESP_LOGE(TAG, "sx_wifi_save_credentials: password too long");
        return false;
    }
    if (!sx_wifi_init()) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SX_WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, SX_WIFI_NVS_KEY_SSID, ssid);
    err = err == ESP_OK ? nvs_set_str(handle, SX_WIFI_NVS_KEY_PASS, password ? password : "") : err;
    err = err == ESP_OK ? nvs_commit(handle) : err;
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Saved Wi-Fi credentials for SSID '%s'", ssid);
    return true;
}

bool sx_wifi_load_credentials(char *ssid_out, size_t ssid_out_len,
                               char *password_out, size_t password_out_len)
{
    if (ssid_out == NULL || password_out == NULL) {
        return false;
    }
    if (!sx_wifi_init()) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SX_WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        /* ESP_ERR_NVS_NOT_FOUND is the expected "no credentials saved
         * yet" case on first boot -- not an error worth logging loudly. */
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "nvs_open (read) failed: %s", esp_err_to_name(err));
        }
        return false;
    }

    size_t ssid_len = ssid_out_len;
    size_t pass_len = password_out_len;
    err = nvs_get_str(handle, SX_WIFI_NVS_KEY_SSID, ssid_out, &ssid_len);
    err = err == ESP_OK ? nvs_get_str(handle, SX_WIFI_NVS_KEY_PASS, password_out, &pass_len) : err;
    nvs_close(handle);

    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "failed to load Wi-Fi credentials: %s", esp_err_to_name(err));
        }
        return false;
    }

    return true;
}

bool sx_wifi_clear_credentials(void)
{
    if (!sx_wifi_init()) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SX_WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err == ESP_ERR_NVS_NOT_FOUND; /* nothing to clear is success */
    }

    err = nvs_erase_all(handle);
    err = err == ESP_OK ? nvs_commit(handle) : err;
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to clear Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

/* ---- policy: STA with AP fallback ------------------------------------ */

bool sx_wifi_start_auto(void)
{
    if (!sx_wifi_init()) {
        ESP_LOGE(TAG, "sx_wifi_start_auto: core init failed");
        return sx_wifi_start_default_ap();
    }

    char ssid[SX_WIFI_SSID_MAX_LEN] = { 0 };
    char password[SX_WIFI_PASSWORD_MAX_LEN] = { 0 };

    if (sx_wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "Found saved credentials for SSID '%s', attempting STA connect", ssid);
        if (sx_wifi_start_sta(ssid, password)) {
            return true;
        }
        ESP_LOGW(TAG, "Saved-credential STA connect failed, falling back to AP mode");
    } else {
        ESP_LOGI(TAG, "No saved Wi-Fi credentials, starting AP mode for configuration");
    }

    sx_wifi_start_default_ap();
    return false;
}