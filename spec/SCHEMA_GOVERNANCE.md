# Schema Governance — Single Source of Truth for the Wire Protocol

This document explains how the wire format (`schema/wire_protocol.json`) is
authored and kept in sync across every language/toolchain a Motor Node or
Display Node might be implemented in, and what a genuinely independent
third-party implementation (a different vendor's motor controller, in its
own repo, its own language) needs to interoperate without ever reading this
repo's C or TypeScript.

This is a governance/architecture document, not a change log — it exists so
"where does the wire format actually come from" has one durable answer, the
same role `COMMS_PROTOCOL_SPEC.md` plays for the transaction model and
safety properties. Read that document first; this one is about *how the
concrete field tables it describes are authored and kept in sync*, not
about the protocol's semantics.

## 1. The problem this repo solves

Without a published, language-neutral schema, the same CRC16 + frame-build
logic (`COMMS_PROTOCOL_SPEC.md` §4's wire format) and the same field
tables tend to get hand-duplicated once per implementation and once per
language — a motor's real firmware, a display's hand-ported copy, and
however many independent copies a web configurator or bench-test tool
ends up with. Each hand-port is a re-derivation done by reading another
implementation's comments and reimplementing the byte math — exactly the
failure mode a real third-party motor vendor would also hit, except
without a spec artifact to derive from at all, they'd have only prose and
someone else's source to work from.

The same problem exists one level up for the **signal catalog**: which
telemetry/command/configuration signals exist, their stable numeric ids,
categories, and wire widths. If that catalog is only a hand-authored enum
inside one specific motor implementation, everything downstream (display
menus, protocol documentation, other implementations) either becomes
generated from — and thus coupled to — one implementation's private
source, or drifts from it silently.

## 2. Why this doesn't scale without a published schema

Java-style versioned libraries solve cross-implementation compatibility via
a *runtime dependency resolver* (Maven/npm/OSGi) that loads a compatible
version at load/link time. That mechanism has no embedded-C equivalent:
a motor and a display are never built together, never linked together, and
never share a process — the only thing they ever share is bytes on a wire.
So the "library" that needs independent versioning here was never a
compiled artifact in the first place; it's the **wire protocol + schema**,
which is exactly what real multi-vendor embedded ecosystems already
version independently of any implementation:

- **MAVLink** (drones): messages defined in XML "dialects," code-generated
  per language/platform; a dialect version is a document, not a binary.
- **CAN DBC / SAE J1939** (automotive): signals defined in a vendor-neutral
  data-dictionary file, independent of any one ECU's firmware — this is
  literally the "N vendors' controllers on one bus" situation.
- **BLE GATT / USB HID**: a capability/characteristic declaration model,
  which `NATIVE_FRAME_CAPABILITY`'s bitmask independently reinvents in
  miniature.

All three share one structural trait: the schema is a **published,
language-neutral artifact**, and every implementation (including this
project's own C and TypeScript) is generated from or validated against it
— never from another implementation's source. That's this repo's whole
reason to exist as something separate from any one firmware/tool project.

## 3. What's here, and what a schema-driven implementation looks like

**One canonical schema file**: `schema/wire_protocol.json`. Contents:

- Frame type enum (`NATIVE_FRAME_ALIVE`, `..._PERIODIC`, `..._COMMAND`,
  `..._CONFIGURATION`, `..._CAPABILITY`, `..._CONFIG_READ`) with their
  numeric values (`COMMS_PROTOCOL_SPEC.md` §4's frame-type byte).
- The signal catalog: every signal's stable numeric id, category
  (raw-sensed/derived-telemetry/command/configuration/control-internal),
  width (1 or 2 bytes), and machine name.
- The wire field tables: telemetry fields, command fields, and the
  configuration-transaction payload shape — each row a `(signal, offset,
  width)` triple, in the real append-only wire order.
- The protocol version (`COMMS_PROTOCOL_SPEC.md` §1 axis #3) as a field
  on the schema itself, with a changelog section recording what changed at
  each bump — the "publish a versioned spec" half of the MAVLink/DBC
  pattern above.

**Generated, never hand-edited, from that one file** (this repo's own
`tools/`):

| Output | Replaces |
|---|---|
| `codec/c/native_telemetry_offsets.h` | Hand-typed offset literals in a C frame parser |
| `codec/ts/native-frame-tables.ts` | Hand-typed frame-type/field numbers in TypeScript tooling |

A consumer project's own signal catalog, config-menu generation, and any
other implementation-specific tooling that needs the same field layout is
expected to generate from or validate against this same
`schema/wire_protocol.json` — the mechanism is the same regardless of
which project is doing the consuming.

**Stays hand-written, everywhere** (the schema only owns wire *layout*, not
behavior): the CRC16 + frame-build/validate/field-read byte-math functions
(`native_crc16`, `native_frame_build`, `native_frame_validate`,
`native_field_read_raw`) are pure, tiny, and stable — shared C source
(`codec/c/native_protocol.c`) for every C-based implementation, and one
shared TypeScript implementation (`codec/ts/native-frame-codec.ts`) for
every TypeScript-based one. There is deliberately no C-to-TS codegen for
this part — see that file's own header comment for why.

Net result: the codec (byte math) is shared source in each of two
language families (C, TypeScript); the *schema* (what fields exist, what
order, what width) is shared data in one JSON file that both language
families generate from. A third-party motor vendor in a different repo,
different language entirely, only ever needs `COMMS_PROTOCOL_SPEC.md` +
`schema/wire_protocol.json` — never this repo's C or TS.

## 4. What this does NOT change

- No runtime version negotiation beyond what already exists
  (`protocol_version`/`schema_version` on the ALIVE frame,
  `COMMS_PROTOCOL_SPEC.md` §1). This document is about *authoring* the
  schema, not renegotiating it live.
- No dynamic linking, no plugin loading, no runtime codegen — every
  generated file above is a build-time step (`python3 tools/gen_*.py`),
  committed to git like any other generated source.
- SDCC/ARM-GCC/TypeScript compatibility is a non-issue for the *data*
  tables (plain arrays of small integers compile trivially anywhere); it
  only matters for the *codec* functions, which this document deliberately
  leaves as hand-written-but-shared-within-each-language-family, not
  schema-generated.
