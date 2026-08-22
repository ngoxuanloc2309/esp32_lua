#ifndef SX_WIFI_H
#define SX_WIFI_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Middle layer of the component -> service -> app chain.
 *
 * Wraps Wi-Fi station-mode connection setup (esp_wifi, esp_netif, the
 * default event loop, and the connect/got-IP event handshake) behind
 * a single blocking call. Deliberately knows nothing about HTTP or
 * Lua -- it only brings the network interface up so that services
 * like http_server (which do not start networking themselves) have
 * something to listen on. SSID/password are supplied by the caller
 * (app/main), not hardcoded here, so this service stays reusable
 * across boards/networks. */

/* Initializes NVS (required by the Wi-Fi driver for calibration data),
 * the default event loop, esp_netif, and the Wi-Fi driver in station
 * mode, then connects to the given access point and blocks until
 * either an IP address is obtained or the connection attempt
 * definitively fails (after internal retries are exhausted).
 *
 * Must be called at most once per boot. Returns true once connected
 * with an IP address; false if the connection could not be
 * established. On success, logs the assigned IP address. */
bool sx_wifi_connect_and_wait(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
#endif /* SX_WIFI_H */