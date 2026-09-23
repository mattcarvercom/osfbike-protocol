# Universal Motor↔Display Communications Protocol — Specification

**Status: v1, implemented.** This is the authoritative,
**motor-implementation-agnostic** specification for the link between a
Motor Node and a Display Node. Nothing in this document assumes any
specific chip or motor family — a Motor Node can be any physical product,
including a controller built on an entirely different MCU/toolchain than
today's reference implementation, and as long as it implements this
specification faithfully, any Display Node implementing the corresponding
client side works against it without modification.

No legacy-compatibility constraint exists anywhere in this design — this is
a from-scratch protocol, not a migration of an older one — so every version
number below starts clean at 1 rather than mapping onto any prior numbering.

## 1. Four independently versioned things

| # | What it versions | Scope | Who cares about it |
|---|---|---|---|
| 1 | **Motor implementation release** | One specific motor controller's compiled firmware (e.g. "TSDZ2/STM8 motor firmware v1.4.0") | Purely informational — shown on a display's info screen, never compared against anything |
| 2 | **Display implementation release** | One specific display's compiled firmware, per target | Purely informational, same as #1 |
| 3 | **Communications protocol version** | **This specification itself** | The one number that actually determines interoperability |
| 4 | **Storage schema version** | Private to one specific motor implementation's own persistent storage | Never crosses the wire, never relevant to any display |

**The relationship that matters:** #1 and #4 are private to whichever motor
happens to be attached; #2 is private to whichever display happens to be
attached; **only #3 is the shared contract that has to match (or be judged
compatible) for a motor and a display to work together at all.** A display
never needs to know or care what #1 or #4 are — it only needs to know whether
the attached motor speaks a protocol version (#3) it understands. This is
precisely what makes "swap the motor for a differently-implemented
controller and the display just works" possible in principle (worked
through concretely in §6).

Most protocol evolution (new optional telemetry/command/config fields)
doesn't even require bumping #3 — the length-prefixed, append-only wire
format (§4) absorbs that. #3 only needs to change for a genuine,
incompatible restructuring of the frame or transaction model — rare by
design.

## 2. What a compliant Motor Node must provide

**Physical-layer baud rate:** a Motor Node's native-protocol UART link must
run at **19200 baud** — a real, fixed property of the display side's own
UART peripheral configuration, not something the display side can
renegotiate. This is a genuine wire-compatibility requirement, not an
implementation detail: a differently-implemented Motor Node that spoke the
correct frame format at the wrong baud would still fail to interoperate
with any existing Display Node, silently — this exact mismatch has caused
real interoperability failures in practice. If a future protocol version
ever needs a different baud, that's a #3 version bump (§1) each side must
negotiate explicitly — never assumed.

A Motor Node is anything that implements all of the following, regardless of
its internal hardware:

- **The three transaction types**: periodic broadcast telemetry
  (unsolicited, motor-initiated), commands (display-initiated, applied by the
  motor), and configuration read/write (either-initiated, always a matched
  reply).
- **A wire-facing signal set**: whichever telemetry/command/configuration
  signals it chooses to expose (never raw-sensed or control-internal
  signals, which stay implementation-private by construction), encoded per
  §4 below.
- **Real safety properties, independent of any implementation detail**:
  brake response within one control cycle regardless of link state,
  continued operation of the last valid command on link loss,
  discard-not-partially-apply on malformed input, single-point validation of
  configuration writes.
- **A capability declaration**: which commands and configuration
  transactions it can accept, so a display never needs to identify a motor by
  name to know what's safe to send it.
- **A protocol version claim** (§1 #3) — which version of this spec it
  implements.

Notably absent from this list: any requirement to speak a legacy
third-party display protocol. Supporting one is an individual
implementation's own choice (a compatibility adapter layered on top), not
part of this specification — see §5.

## 3. What a compliant Display Node must provide

Mirror image of §2:

- Parses the wire-facing signal set per §4, tolerating any signal it doesn't
  recognize (per the length-prefixed format — an unrecognized trailing field
  is simply absent, never an error).
- Originates only the commands/configuration writes its own capability
  declaration to the motor claims it will.
- Never branches behavior on a motor's implementation release version (§1
  #1) — only on the protocol version (§1 #3) if it needs to know anything at
  all, which per §1 should be rare.

## 4. Wire format

| Bytes | Content |
|---|---|
| 0 | STX |
| 1 | Frame type |
| 2 | Payload length `N` |
| 3 .. 3+N-1 | Payload, fields in fixed, append-only order |
| last 2 bytes | CRC16 |

Receiver rule: read field *X* only if `N >= cumulative_offset(X) + width(X)`;
otherwise treat it as absent and use its defined default. This is the
entire extensibility mechanism: a new field can be appended to the wire
format without breaking any receiver that predates it, and an older sender
talking to a newer receiver simply produces "absent, use default" for
fields it doesn't know about.

## 5. The legacy-compatibility boundary is explicitly outside this spec

Stock/third-party display protocols (whatever a given motor family's
existing installed base of non-osf.bike displays happens to speak) are a
separate, fixed, third-party protocol that a motor implementation may
happen to also support via compatibility adapters, entirely orthogonal to
this specification. **A Motor Node has zero obligation to support any
legacy dialect** — it only needs to implement §2-4 above to be a fully
compliant Motor Node that any osf.bike display firmware works with.
Whether it *additionally* chooses to support legacy dialects (for its own,
unrelated reasons — e.g. supporting an existing third-party display
directly) is entirely up to that implementation and has no bearing on
protocol compliance.

## 6. Worked example: swapping in a differently-implemented Motor Node

Concretely, what "the display just works" means and doesn't mean:

- A new Motor Node implementation is free to have its own sensors, its own
  control loop, its own storage schema (§1 #4, entirely private — could be
  flash-based key-value storage instead of EEPROM, doesn't matter). None of
  that is visible across the wire.
- It exposes whatever subset of the wire-facing signal set (§2) its own
  hardware can actually produce — e.g. if the hardware doesn't have a
  torque sensor wired the same way, it simply doesn't populate that signal,
  or populates it with a defined "not available" representation.
- Any display already implementing §3 against this spec will correctly show
  whatever signals the new implementation *does* provide, and correctly
  originate commands its capability declaration says it accepts —
  **without a single display-side code change**, because nothing in the
  display's implementation ever depended on which specific motor hardware
  was attached.
- What "just works" does **not** mean: a capability genuinely unique to one
  motor implementation, with no equivalent in the shared signal set, won't
  magically appear on an existing display — that requires extending the
  shared signal set (§2, via the append-only mechanism in §4) and,
  eventually, a display update to render it. The claim is "the existing
  shared contract works unmodified," not "every future motor capability is
  retroactively supported by every existing display."

This is the concrete test for whether this specification is doing its job:
if implementing a new Motor Node ever requires touching *any* existing
Display Node's source to make basic telemetry/commands work, this
specification has failed at being genuinely motor-agnostic, and that failure
should be fixed here, not patched around in a specific display's code.
