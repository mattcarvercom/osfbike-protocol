# osfbike-protocol — guidance for Claude Code

Read `README.md` first for what this repo is and how it's consumed. This
file is about working *in* this repo specifically.

## What this repo is, in one sentence

The vendor-neutral wire-format contract (`schema/wire_protocol.json` +
reference C/TS codecs) that lets any osf.bike-compatible motor controller
talk to any osf.bike-compatible display — see `spec/COMMS_PROTOCOL_SPEC.md`
for the protocol's semantics and `spec/SCHEMA_GOVERNANCE.md` for how the
schema is authored/generated and why it's a separate published artifact.

## Two remotes

- `origin` — this repo, `mattcarvercom/osfbike-protocol-private` if you're
  in the private clone, `mattcarvercom/osfbike-protocol` if you're in the
  public one. Private is the day-to-day working repo (push freely, no PR
  requirement); public gets curated updates once something's ready. There's
  no standing exclusion list between them — nothing about this repo's
  content is private-only, the private remote mostly exists as an offsite
  working copy.

## Editing the schema

`schema/wire_protocol.json` is the single source of truth. After editing
it:

```sh
python3 tools/gen_native_telemetry_offsets_h.py
python3 tools/gen_native_frame_tables_ts.py
```

and commit the regenerated `codec/c/native_telemetry_offsets.h` /
`codec/ts/native-frame-tables.ts` alongside the schema change — both are
committed, generated output, never hand-edited (each file's own header
comment says so).

**A schema change here doesn't take effect in any consumer until that
consumer bumps its `vendor/osfbike-protocol` submodule pointer** — this
repo has no consumers compiled into it directly to catch a breaking change
immediately. If you're editing the schema because of a specific consumer's
need, after pushing here, go bump the submodule pointer in that project and
run its own real builds/tests before considering the change done — nothing
in this repo alone proves a schema change is safe for every consumer.

## The C codec's toolchain constraint

`codec/c/native_protocol.c`/`.h` need to compile under whatever toolchains
motor/display firmware actually use in practice — SDCC (STM8), ARM GCC
(STM32/nRF51-class MCUs), and Emscripten among them. Keep any change here
in conservative, portable C — no toolchain-specific extensions, nothing
that compiles cleanly on only one of those. There's no build/test harness
in this repo itself to catch a violation; the first signal would be a
consumer's build breaking after a submodule bump.

## No CI here yet

This repo has no `.github/workflows/` of its own. Validation happens in
whatever consumer project actually builds against a given commit (see
above) - there isn't a standalone way to "test" this repo in isolation
beyond running the two generators and confirming they still succeed.
