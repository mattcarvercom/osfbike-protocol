/*
 * osf.bike native protocol — shared wire codec.
 *
 * Single source for the byte-level mechanics of the wire format
 * (spec/COMMS_PROTOCOL_SPEC.md §4, spec/SCHEMA_GOVERNANCE.md): the STX/
 * type/length/payload/CRC16 frame shape, CRC16 itself, and the
 * length-gated field-read rule. Meant to be compiled directly (as a git
 * submodule, see this repo's own README.md) into BOTH a motor
 * controller's firmware and any display firmware that needs to
 * build/parse a native frame, instead of each side hand-porting its own
 * copy.
 *
 * Deliberately has ZERO dependency on any specific motor's signal
 * catalog or any storage/UART/peripheral code — a display firmware that
 * never links a motor's own source can still compile this file
 * unmodified. native_field_read_raw() below takes a plain (offset,
 * width) pair rather than a signal-catalog-typed field descriptor for
 * exactly this reason; a caller with a richer, catalog-typed field table
 * of its own wraps this function rather than replacing it.
 *
 * Frame layout (spec/COMMS_PROTOCOL_SPEC.md §4):
 *
 *   byte 0    STX (0x43)
 *   byte 1    frame type
 *   byte 2    payload length N
 *   bytes 3..3+N-1   payload, fields in fixed append-only order
 *   last 2    CRC16 (poly 0xA001, init 0xFFFF, little-endian)
 *
 * The receiver rule (the entire extensibility mechanism): a receiver may
 * only read field X if N >= offset(X) + width(X); otherwise X is absent
 * and its defined default is used. This is what lets a new field be
 * appended to the wire format without breaking any receiver that
 * predates it.
 */

#ifndef OSFBIKE_NATIVE_PROTOCOL_H
#define OSFBIKE_NATIVE_PROTOCOL_H

#include <stdint.h>

/* The STX byte. */
#define NATIVE_STX 0x43u

/* The wire's maximum payload length — 255 (one length byte's range) is the
 * honest cap: a receiver must treat a frame whose claimed N exceeds this
 * as malformed. */
#define NATIVE_MAX_PAYLOAD 255u

/* The frame buffer size: STX + type + length + max payload + CRC16. */
#define NATIVE_FRAME_SIZE (3 + NATIVE_MAX_PAYLOAD + 2)

/* Frame types (COMMS_PROTOCOL_SPEC.md §4's frame-type byte; also
 * schema/wire_protocol.json's "frame_types" — keep both in sync). */
typedef enum {
    NATIVE_FRAME_ALIVE = 0,
    NATIVE_FRAME_PERIODIC = 1,     /* broadcast telemetry, motor -> display */
    NATIVE_FRAME_COMMAND = 2,      /* display -> motor */
    NATIVE_FRAME_CONFIGURATION = 3,/* config read/write, either direction */
    NATIVE_FRAME_CAPABILITY = 4,   /* capability declaration */
    NATIVE_FRAME_CONFIG_READ = 5   /* config-read request */
} native_frame_type_t;

/* Compute the CRC16 the wire uses (poly 0xA001, init 0xFFFF) over a byte
 * range. */
uint16_t native_crc16(const uint8_t *data, uint8_t len);

/* Build a frame into buf (must hold NATIVE_FRAME_SIZE bytes): STX, type,
 * the payload's length, the payload bytes, the CRC16 (little-endian at
 * the end). Returns the total frame length (3 + N + 2), or 0 if N exceeds
 * NATIVE_MAX_PAYLOAD. */
uint8_t native_frame_build(uint8_t *buf, native_frame_type_t type,
                           const uint8_t *payload, uint8_t n);

/* Parse (validate) a received frame in buf (len bytes received). Checks
 * the STX byte, the claimed length fits the received bytes, and the
 * CRC16. Returns 1 if structurally valid (*out_n receives the payload
 * length), 0 otherwise (discard). */
int native_frame_validate(const uint8_t *buf, uint8_t len, uint8_t *out_n);

/* Read one wire field at a given (offset, width) from a validated
 * frame's payload, per the receiver rule above. Returns 1 and sets *out
 * if N covers the field; returns 0 (field absent — the caller uses its
 * defined default) otherwise. width must be 1 or 2. */
int native_field_read_raw(const uint8_t *payload, uint8_t n,
                          uint8_t offset, uint8_t width, uint16_t *out);

/* ------------------------------------------------------------------ */
/* ALIVE frame                                                         */
/* ------------------------------------------------------------------ */

/* Build an ALIVE frame with the protocol version and the config-schema
 * version as its 2-byte payload. Payload: [protocol_version,
 * schema_version]. Pure envelope function — no signal-catalog dependency
 * (unlike a richer, catalog-typed field-read wrapper), so it lives here
 * rather than in any one implementation's own field-table header, usable
 * by any Motor Node OR Display Node implementation. Returns the frame
 * length (3 + 2 + 2 = 7). */
uint8_t native_alive_build(uint8_t *buf, uint8_t protocol_version,
                            uint8_t schema_version);

/* Parse a received ALIVE frame: extract the protocol version and (if
 * present, n >= 2) the config-schema version. Append-only, same rule as
 * every other field on the wire: n >= 1 means protocol version present;
 * n >= 2 means schema version also present. If n < 2, *out_schema_version
 * is left unchanged (the caller shows "?" for it). Returns 1 if the
 * payload is at least 1 byte, 0 otherwise (discard). */
int native_alive_parse(const uint8_t *payload, uint8_t n,
                       uint8_t *out_protocol_version,
                       uint8_t *out_schema_version);

#endif /* OSFBIKE_NATIVE_PROTOCOL_H */
