# osfbike-protocol

The **osf.bike native wire protocol**: a vendor-neutral schema and reference
codec for the UART link between a motor controller ("Motor Node") and a
display ("Display Node") in the [osf.bike](https://osf.bike) ecosystem.

The goal: any supported display, running osf.bike firmware, should work with
**any** supported motor controller — swap the motor, keep the display, no
reflash needed — because both sides are built against the same published
contract instead of a project-private implementation detail. Today that
means [`osfbike-tsdz2`](https://github.com/mattcarvercom/osfbike-tsdz2)'s
TSDZ2 motor firmware and its 860C/850C/SW102 display firmwares; the same
contract is meant to cover future motor controllers (TSDZ8, TSDZ16, Bafang
BB-series, ...) without those needing to fork or reimplement it from prose.

## What's here

| Path | What |
|---|---|
| `spec/COMMS_PROTOCOL_SPEC.md` | The protocol's semantics: transaction model, frame types, safety properties, versioning. Read this first. |
| `spec/SCHEMA_GOVERNANCE.md` | How the schema below is authored/generated, and why it's a separate published artifact rather than something inferred from any one implementation's source. |
| `schema/wire_protocol.json` | The single source of truth for the wire format: frame types, the signal catalog, and the telemetry/command field tables (name, wire offset, width, order). Everything else here is generated from or checked against this file. |
| `codec/c/` | Reference C codec (frame build/validate, CRC16, field read) plus the schema-generated `native_telemetry_offsets.h`. Conservative C89/C99, no toolchain-specific extensions — compiles under SDCC (STM8), ARM GCC, and Emscripten today. |
| `codec/ts/` | Reference TypeScript codec (`native-frame-codec.ts`, hand-written) and its schema-generated field/frame-type tables (`native-frame-tables.ts`). |
| `tools/` | The two generators that produce the schema-derived files above (`gen_native_telemetry_offsets_h.py`, `gen_native_frame_tables_ts.py`). Re-run after editing `schema/wire_protocol.json` and commit the result — these outputs are committed, not built on the fly. |

A from-scratch third-party implementation, in its own repo and language,
only ever needs `spec/COMMS_PROTOCOL_SPEC.md` + `schema/wire_protocol.json`
— never this repo's C or TypeScript.

## Using this from a firmware/tool repo

Vendor it as a git submodule:

```sh
git submodule add https://github.com/mattcarvercom/osfbike-protocol.git vendor/osfbike-protocol
```

Then `#include` `codec/c/native_protocol.h` (C) or import from
`codec/ts/native-frame-codec.ts` (TypeScript) directly from the submodule
path — no build step of this repo's own is required to consume it.

`osfbike-tsdz2`'s TSDZ2 motor firmware and its 860C/850C/SW102 display
firmwares are the current reference consumers of this schema/codec.

## License

GPLv3 (`LICENSE`) — matching `osfbike-tsdz2`, deliberately: this project
exists because motor/display manufacturers already ship closed firmware,
and copyleft is what keeps anyone building on this community's work
reciprocating rather than forking it into a closed commercial product for
free.
