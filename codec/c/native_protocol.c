/*
 * osf.bike native protocol — shared wire codec implementation.
 *
 * See native_protocol.h for the design references. Pure byte math only —
 * no UART, no peripheral access, no signal-catalog access, no #if. The
 * same codec compiles unmodified under SDCC (STM8), ARM GCC (STM32,
 * nRF51), and a native x86 test harness.
 */

#include "native_protocol.h"

/* ------------------------------------------------------------------ */
/* CRC16 (poly 0xA001, init 0xFFFF — the real 860C/850C CRC this        */
/* motor/display family's UART link already used before this repo      */
/* existed; kept byte-for-byte identical, not re-derived)               */
/* ------------------------------------------------------------------ */

/* The per-byte CRC step, kept verbatim — it is a verified wire fact,
 * not an algorithm to re-derive. */
static void crc16_byte(uint8_t data, uint16_t *crc)
{
    unsigned int i;
    *crc = *crc ^ (uint16_t)data;
    for (i = 8; i > 0; i--) {
        if (*crc & 0x0001) {
            *crc = (*crc >> 1) ^ 0xA001;
        } else {
            *crc >>= 1;
        }
    }
}

uint16_t native_crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF; /* the verified initial value */
    for (uint8_t i = 0; i < len; i++) {
        crc16_byte(data[i], &crc);
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* The codec                                                           */
/* ------------------------------------------------------------------ */

uint8_t native_frame_build(uint8_t *buf, native_frame_type_t type,
                           const uint8_t *payload, uint8_t n)
{
    if (n > NATIVE_MAX_PAYLOAD) {
        return 0;
    }
    buf[0] = (uint8_t)NATIVE_STX;
    buf[1] = (uint8_t)type;
    buf[2] = n;
    for (uint8_t i = 0; i < n; i++) {
        buf[3 + i] = payload[i];
    }
    uint16_t crc = native_crc16(buf, (uint8_t)(3 + n));
    buf[3 + n + 0] = (uint8_t)crc;         /* little-endian: low byte first */
    buf[3 + n + 1] = (uint8_t)(crc >> 8);
    return (uint8_t)(3 + n + 2);
}

int native_frame_validate(const uint8_t *buf, uint8_t len, uint8_t *out_n)
{
    /* The minimum structurally-valid frame: STX + type + length + CRC,
     * no payload. */
    if (len < 5) {
        return 0;
    }
    if (buf[0] != (uint8_t)NATIVE_STX) {
        return 0;
    }
    uint8_t n = buf[2];
    if (n > NATIVE_MAX_PAYLOAD) {
        return 0; /* a corrupt length byte must not index past the buffer */
    }
    /* The received bytes must cover the whole claimed frame. */
    if (len < (uint8_t)(3 + n + 2)) {
        return 0;
    }
    /* CRC over everything before it. */
    uint16_t crc = native_crc16(buf, (uint8_t)(3 + n));
    uint16_t received = (uint16_t)buf[3 + n + 0] | ((uint16_t)buf[3 + n + 1] << 8);
    if (crc != received) {
        return 0;
    }
    *out_n = n;
    return 1;
}

int native_field_read_raw(const uint8_t *payload, uint8_t n,
                          uint8_t offset, uint8_t width, uint16_t *out)
{
    /* The receiver rule (the entire extensibility mechanism): read the
     * field only if the claimed N covers it; otherwise it is absent
     * (the caller uses its defined default). An absent field is not an
     * error — it is the normal "older sender" state. */
    if (n < (uint8_t)(offset + width)) {
        return 0;
    }
    if (width == 1) {
        *out = payload[offset];
    } else {
        *out = (uint16_t)payload[offset] | ((uint16_t)payload[offset + 1] << 8);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* ALIVE frame                                                         */
/* ------------------------------------------------------------------ */

uint8_t native_alive_build(uint8_t *buf, uint8_t protocol_version,
                            uint8_t schema_version)
{
    /* 2-byte payload [protocol_version, schema_version]. */
    uint8_t payload[2];
    payload[0] = protocol_version;
    payload[1] = schema_version;
    return native_frame_build(buf, NATIVE_FRAME_ALIVE, payload, 2);
}

int native_alive_parse(const uint8_t *payload, uint8_t n,
                       uint8_t *out_protocol_version,
                       uint8_t *out_schema_version)
{
    /* Minimum: 1 byte (protocol version). n < 1 means the field is
     * absent (a malformed or ancient ALIVE) — never an out-of-bounds
     * read. */
    if (n < 1) {
        return 0;
    }
    *out_protocol_version = payload[0];
    /* Append-only, same rule as every other field on the wire: the
     * schema version is present only if n >= 2. An older motor that
     * sends a 1-byte ALIVE is still valid — the schema version is
     * simply absent (the caller shows "?"). */
    if (n >= 2) {
        *out_schema_version = payload[1];
    }
    return 1;
}
