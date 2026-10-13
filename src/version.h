#ifndef ESP_GW_VERSION_H
#define ESP_GW_VERSION_H

// Bump on each build so the boot banner confirms which firmware is actually
// running after an OTA (if it doesn't change, the OTA didn't switch slots).
// Also exposed read-only by GET /config and shown in the web UI header.
#define FW_VERSION "1.3.4"

#endif
