#include "hm10_packet.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Advances a string pointer past spaces and tab characters.
 * @param text Pointer to a null-terminated string; NULL is not allowed.
 * @return Pointer to the first non-space, non-tab character in text.
 */
static const char* HM10_PacketSkipSpaces(const char* text)
{
    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    return text;
}

/**
 * @brief Consumes one expected character from a parsed text cursor.
 * @param cursor Pointer to the current parse cursor; cursor and *cursor must
 *               not be NULL and are advanced on success.
 * @param expected Character that must appear after optional spaces.
 * @return 1 when the expected character is found and consumed, or 0 when the
 *         cursor is invalid or the expected character is not present.
 */
static uint8_t HM10_PacketConsumeChar(const char** cursor, char expected)
{
    const char* p;

    if ((cursor == NULL) || (*cursor == NULL))
    {
        return 0U;
    }

    p = HM10_PacketSkipSpaces(*cursor);

    if (*p != expected)
    {
        return 0U;
    }

    *cursor = HM10_PacketSkipSpaces(p + 1);
    return 1U;
}

/**
 * @brief Reads one floating-point value from a parsed text cursor.
 * @param cursor Pointer to the current parse cursor; cursor and *cursor must
 *               not be NULL and are advanced on success.
 * @param value Output pointer that receives the parsed double; NULL is not allowed.
 * @return 1 when a double value is parsed successfully, or 0 when arguments are
 *         invalid or no valid number is found.
 */
static uint8_t HM10_PacketReadDouble(const char** cursor, double* value)
{
    char* end = NULL;
    const char* start;

    if ((cursor == NULL) || (*cursor == NULL) || (value == NULL))
    {
        return 0U;
    }

    start = HM10_PacketSkipSpaces(*cursor);
    *value = strtod(start, &end);

    if (end == start)
    {
        return 0U;
    }

    *cursor = HM10_PacketSkipSpaces(end);
    return 1U;
}

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
uint8_t HM10_ParseDataPacket(const char* text, HM10_DataPacket* packet)
{
    const char* cursor;

    if ((text == NULL) || (packet == NULL))
    {
        return 0U;
    }

    memset(packet, 0, sizeof(*packet));
    cursor = text;

    if (!HM10_PacketConsumeChar(&cursor, '[')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->lat_deg)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ',')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->lon_deg)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ',')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->alt_m)) return 0U;

    if (HM10_PacketConsumeChar(&cursor, ','))
    {
        packet->has_hacc = 1U;

        if (!HM10_PacketReadDouble(&cursor, &packet->hacc_m)) return 0U;
        if (!HM10_PacketConsumeChar(&cursor, ';')) return 0U;
    }
    else
    {
        packet->has_hacc = 0U;

        if (!HM10_PacketConsumeChar(&cursor, ';')) return 0U;
    }

    if (!HM10_PacketReadDouble(&cursor, &packet->qw)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ',')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->qx)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ',')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->qy)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ',')) return 0U;
    if (!HM10_PacketReadDouble(&cursor, &packet->qz)) return 0U;
    if (!HM10_PacketConsumeChar(&cursor, ']')) return 0U;

    return (*HM10_PacketSkipSpaces(cursor) == '\0') ? 1U : 0U;
}
