#ifndef SX_WIFI_H
#define SX_WIFI_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Middle layer of the component -> service -> app chain.
 *
 * Wraps Wi-Fi setup (esp_wifi, esp_netif, the default event loop, NVS
 * for both Wi-Fi calibration data and saved STA credentials) behind a
 * small set of calls. Deliberately knows nothing about HTTP or Lua --
 * it only brings up networking so that services like http_server
 * (which do not start networking themselves) have something to
 * listen on.
 *
 * Policy (which mode to use, when to fall back) lives in the app
 * layer via sx_wifi_start_auto(); this service only provides the
 * mechanism (start_sta / start_ap / credential storage) so it stays
 * reusable across boards and provisioning flows. */

#define SX_WIFI_SSID_MAX_LEN     32   /* IEEE 802.11 SSID limit */
#define SX_WIFI_PASSWORD_MAX_LEN 64   /* WPA2-PSK passphrase limit */

/* Default SoftAP identity used by sx_wifi_start_auto() when no STA
 * connection could be established. Open network (no password) so a
 * phone/laptop can join without prior knowledge -- the device is
 * expected to be physically accessible when this path is hit (first
 * boot / lost credentials), and the AP only serves the local
 * configuration web UI, not production traffic. */
#define SX_WIFI_DEFAULT_AP_SSID_PREFIX "ESP32-LUA-"   /* + last 3 bytes of MAC */
#define SX_WIFI_DEFAULT_AP_CHANNEL      1
#define SX_WIFI_DEFAULT_AP_MAX_CONN     4

/* One-time setup shared by every mode below: NVS (required by the
 * Wi-Fi driver itself for calibration data, and by this service for
 * saved STA credentials), the default event loop, and esp_netif.
 * Must be called exactly once before any sx_wifi_start_*() call.
 * Returns true on success. Safe to treat NVS-needs-erase as handled
 * internally (an out-of-date NVS partition layout is erased and
 * re-initialized automatically, matching common ESP-IDF examples). */
bool sx_wifi_init(void);

/* Connects to the given access point in station mode and blocks
 * until either an IP address is obtained or the connection attempt
 * definitively fails (after internal retries are exhausted). Starts
 * the Wi-Fi driver in STA mode as part of this call.
 *
 * Returns true once connected with an IP address; false if the
 * connection could not be established (bad credentials, AP out of
 * range, etc). On success, logs the assigned IP address. */
bool sx_wifi_start_sta(const char *ssid, const char *password);

/* Starts a SoftAP with the given SSID/password and returns
 * immediately (does not block waiting for clients). Pass password ==
 * NULL or "" for an open (unsecured) network; a non-empty password
 * must be at least 8 characters (WPA2-PSK minimum) or this call
 * fails. Starts the Wi-Fi driver in AP mode as part of this call.
 *
 * Returns true once the AP is up. */
bool sx_wifi_start_ap(const char *ap_ssid, const char *ap_password);

/* Convenience wrapper around sx_wifi_start_ap() that derives a unique
 * SSID from the device's Wi-Fi MAC address
 * (SX_WIFI_DEFAULT_AP_SSID_PREFIX + last 3 MAC bytes as hex) and
 * starts an open network. Used by sx_wifi_start_auto()'s fallback
 * path and available directly for an app that wants "factory reset /
 * force config mode" behavior. */
bool sx_wifi_start_default_ap(void);

/* Saves STA credentials to NVS so a future sx_wifi_load_credentials()
 * (including the one inside sx_wifi_start_auto()) picks them up.
 * Intended to be called from the app-layer HTTP handler that accepts
 * Wi-Fi configuration from the user (e.g. POST /wifi_config), after
 * which the app typically reboots or calls sx_wifi_start_sta() to
 * apply them immediately.
 *
 * ssid/password must fit within SX_WIFI_SSID_MAX_LEN /
 * SX_WIFI_PASSWORD_MAX_LEN (including the NUL terminator). Returns
 * true on success. */
bool sx_wifi_save_credentials(const char *ssid, const char *password);

/* Loads previously saved STA credentials from NVS into the given
 * output buffers (both NUL-terminated on success). Returns false if
 * no credentials have ever been saved (first boot) or the NVS read
 * fails; in that case *ssid_out/*password_out are left untouched. */
bool sx_wifi_load_credentials(char *ssid_out, size_t ssid_out_len,
                               char *password_out, size_t password_out_len);

/* Erases any saved STA credentials from NVS. Used by a future
 * "factory reset" / "forget network" app-layer action. Returns true
 * if the erase succeeded or there was nothing to erase. */
bool sx_wifi_clear_credentials(void);

/* Policy entrypoint intended for app_main(): loads saved STA
 * credentials and attempts sx_wifi_start_sta(). If no credentials are
 * saved, or the connection attempt fails, falls back to
 * sx_wifi_start_default_ap() so the device is always reachable
 * (either on the configured network, or via its own AP for
 * reconfiguration).
 *
 * Returns true if the device ends up connected in STA mode, false if
 * it fell back to AP mode. Either way the device has a usable network
 * interface for sx_http_server_start() by the time this returns --
 * callers only need to branch on the return value if they want to
 * log/display which mode is active. */
bool sx_wifi_start_auto(void);

#ifdef __cplusplus
}
#endif
#endif /* SX_WIFI_H */