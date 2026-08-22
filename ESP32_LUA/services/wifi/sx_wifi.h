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
 * This service provides mechanism only: start_sta(), start_ap(),
 * credential save/load. It has no opinion on which mode to use, when
 * to fall back from STA to AP, or when credentials should be loaded
 * versus discarded -- all of that is policy, decided entirely by the
 * caller (main/esp_lua.c). This service will never call itself; it
 * only ever runs a function when the app layer calls it. */

#define SX_WIFI_SSID_MAX_LEN     32   /* IEEE 802.11 SSID limit */
#define SX_WIFI_PASSWORD_MAX_LEN 64   /* WPA2-PSK passphrase limit */

/* Default SoftAP identity. This is just a constant the app layer may
 * use when calling sx_wifi_start_ap() / building an SSID -- the
 * service does not use it itself and has no "default AP" behavior of
 * its own. */
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

/* Reads the device's Wi-Fi MAC address and formats it into a unique
 * SSID as SX_WIFI_DEFAULT_AP_SSID_PREFIX + last 3 MAC bytes (hex).
 * Pure helper: does not start anything, does not touch NVS or the
 * Wi-Fi driver's mode. The app layer calls this to build an SSID to
 * pass into sx_wifi_start_ap() if it wants a MAC-derived name;
 * nothing requires it be used. out_len should be at least
 * SX_WIFI_SSID_MAX_LEN. Returns true on success. */
bool sx_wifi_build_default_ap_ssid(char *ssid_out, size_t ssid_out_len);

/* Saves STA credentials to NVS so a future sx_wifi_load_credentials()
 * call picks them up. Pure storage -- does not start, stop, or affect
 * any currently running Wi-Fi mode. The app layer decides when to
 * call this (e.g. from an HTTP handler that accepts Wi-Fi
 * configuration from the user) and what to do afterwards (reboot,
 * call sx_wifi_start_sta() immediately, etc).
 *
 * ssid/password must fit within SX_WIFI_SSID_MAX_LEN /
 * SX_WIFI_PASSWORD_MAX_LEN (including the NUL terminator). Returns
 * true on success. */
bool sx_wifi_save_credentials(const char *ssid, const char *password);

/* Loads previously saved STA credentials from NVS into the given
 * output buffers (both NUL-terminated on success). Returns false if
 * no credentials have ever been saved (first boot) or the NVS read
 * fails; in that case ssid_out and password_out are left untouched.
 * Pure storage read -- does not start anything. The app layer decides
 * what to do with the result (call start_sta, fall back to start_ap,
 * etc). */
bool sx_wifi_load_credentials(char *ssid_out, size_t ssid_out_len,
                               char *password_out, size_t password_out_len);

/* Erases any saved STA credentials from NVS. Used by an app-layer
 * "factory reset" / "forget network" action. Returns true if the
 * erase succeeded or there was nothing to erase. */
bool sx_wifi_clear_credentials(void);

#ifdef __cplusplus
}
#endif
#endif /* SX_WIFI_H */