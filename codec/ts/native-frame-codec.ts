// osf.bike native-protocol shared wire codec (spec/SCHEMA_GOVERNANCE.md) -
// the TypeScript sibling of codec/c/native_protocol.c. This exists so any
// TypeScript tooling that needs to build or parse a native frame (a bench
// motor emulator, a live config-write client, a web-based flashing tool)
// has exactly one implementation to import instead of each hand-porting
// its own copy of the same ~15 lines of CRC16/frame-build bit math - the
// same "single source of truth" reasoning as codec/c/native_protocol.c
// on the C side.
//
// Deliberately NOT code-generated from wire_protocol.json (unlike
// native-frame-tables.ts, this file's sibling) - the actual byte-math
// algorithm doesn't change when a signal/field is added to the schema, so
// there is nothing here for a schema-driven generator to keep in sync;
// generating ~15 lines of CRC bit-twiddling from JSON would be generating
// for its own sake, not eliminating real drift risk.

import { NATIVE_STX } from "./native-frame-tables.ts";

/** Modbus CRC16 (poly 0xA001, init 0xFFFF, LSB-first) - byte-for-byte port
 * of native_protocol.c's native_crc16(). */
export function crc16(bytes: ArrayLike<number>): number {
  let crc = 0xffff;
  for (let i = 0; i < bytes.length; i++) {
    crc ^= bytes[i];
    for (let bit = 0; bit < 8; bit++) {
      crc = crc & 1 ? (crc >>> 1) ^ 0xa001 : crc >>> 1;
    }
  }
  return crc & 0xffff;
}

/** Builds one native-protocol frame: STX TYPE LEN payload... CRC_LO CRC_HI -
 * port of native_protocol.c's native_frame_build(). */
export function buildNativeFrame(type: number, payload: ArrayLike<number>): Uint8Array {
  const n = payload.length;
  const frame = new Uint8Array(3 + n + 2);
  frame[0] = NATIVE_STX;
  frame[1] = type;
  frame[2] = n;
  for (let i = 0; i < n; i++) frame[3 + i] = payload[i];
  const crc = crc16(frame.subarray(0, 3 + n));
  frame[3 + n] = crc & 0xff;
  frame[3 + n + 1] = (crc >> 8) & 0xff;
  return frame;
}

export interface NativeFrame {
  type: number;
  payload: Uint8Array;
  /** The complete, original wire bytes (STX + type + length + payload + CRC16). */
  raw: Uint8Array;
}

/**
 * A streaming frame reader mirroring native_protocol.c's own framing rule
 * (native_frame_validate(): STX, a length byte the received bytes must
 * actually cover, CRC16 over everything before it) - necessary because Web
 * Serial delivers arbitrary chunk boundaries, not one frame per read,
 * unlike the real UART ISR's byte-at-a-time buffer. Bytes are appended as
 * they arrive; feed() returns every complete, CRC-valid frame found so
 * far. A byte that can't start a valid frame at the front of the buffer is
 * dropped one at a time (resync), the same tolerance a real link needs
 * against a torn first read rather than discarding everything buffered.
 */
export class NativeFrameReader {
  private buf: number[] = [];

  feed(bytes: Uint8Array): NativeFrame[] {
    for (const b of bytes) this.buf.push(b);
    const frames: NativeFrame[] = [];
    for (;;) {
      if (this.buf.length < 5) break; // STX + type + length + CRC(2), minimum
      if (this.buf[0] !== NATIVE_STX) {
        this.buf.shift();
        continue;
      }
      const n = this.buf[2];
      const total = 3 + n + 2;
      if (this.buf.length < total) break; // frame not fully arrived yet
      const frameBytes = this.buf.slice(0, total);
      const crc = crc16(frameBytes.slice(0, 3 + n));
      const received = frameBytes[3 + n] | (frameBytes[3 + n + 1] << 8);
      if (crc !== received) {
        this.buf.shift(); // not really a frame start after all - resync
        continue;
      }
      frames.push({
        type: frameBytes[1],
        payload: Uint8Array.from(frameBytes.slice(3, 3 + n)),
        raw: Uint8Array.from(frameBytes),
      });
      this.buf.splice(0, total);
    }
    return frames;
  }
}
