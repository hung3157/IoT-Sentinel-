#include "radio_observation.h"

String formatMac(const uint8_t mac[6]) {
    char buff[18];
    snprintf(buff, sizeof(buff), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buff);
}
