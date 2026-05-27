#ifndef HM10_PACKET_H
#define HM10_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Stores one parsed phone data packet received through the HM-10 BLE UART bridge.
 *
 * Fields:
 * - lat_deg: Phone latitude in decimal degrees.
 * - lon_deg: Phone longitude in decimal degrees.
 * - alt_m: Phone altitude in meters.
 * - hacc_m: Horizontal location accuracy in meters.
 * - qw/qx/qy/qz: Phone orientation quaternion components.
 * - has_hacc: Nonzero when hacc_m was present in the packet; zero when the
 *   packet omitted horizontal accuracy.
 */
typedef struct
{
    double lat_deg;
    double lon_deg;
    double alt_m;
    double hacc_m;
    double qw;
    double qx;
    double qy;
    double qz;
    uint8_t has_hacc;
} HM10_DataPacket;

/**
 * @brief Parses one HM-10 phone data packet into numeric location and rotation fields.
 * @param text Null-terminated packet string to parse; NULL is not allowed and
 *             must match [lat,lon,alt,hacc;qw,qx,qy,qz] or [lat,lon,alt;qw,qx,qy,qz].
 * @param packet Output packet structure that receives parsed values; NULL is
 *               not allowed and is cleared before parsing.
 * @return 1 when the full packet is parsed successfully, or 0 when arguments
 *         are invalid, the packet format is invalid, or extra non-space text
 *         remains after the closing bracket.
 */
uint8_t HM10_ParseDataPacket(const char* text, HM10_DataPacket* packet);

#ifdef __cplusplus
}
#endif

#endif /* HM10_PACKET_H */
