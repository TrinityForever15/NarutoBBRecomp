# Cutscene audio investigation plan

Date: 2026-07-25. Status: root cause established; final fix incomplete.

## Reported problem

Several story cutscenes degrade their audio. The maintainer reports four
distinct symptoms: audio missing, audio cut off, crackling, and audio repeating
until the cutscene ends.

Three facts from the maintainer shape this plan:

1. At least one cutscene fails **reproducibly**, always the same way.
2. **Different scenes show different symptoms.** This is treated as evidence of
   more than one root cause until measurement says otherwise.
3. **Video stays correct while audio fails.** The guest is therefore not blocked
   waiting on the decoder, which separates these cases from the historical
   AUDIO-07 indefinite wait.

## What is already established

- The SDL driver does not repeat buffers. On starvation it writes silence and
  ramps the last sample down over `kConcealFadeSamples`. The repetition the
  maintainer hears is produced upstream, in the XMA layer.
- `XmaContext` implements real loop semantics: `loop_count`, `loop_start`,
  `loop_end`, with 255 meaning infinite. `UpdateLoopStatus` rewinds
  `input_buffer_read_offset` to `loop_start` and decrements `loop_count` only
  when it is below 255.
- The guest feeds the decoder by alternating `input_buffer_0` and
  `input_buffer_1`; `current_buffer` flips when a buffer is consumed.
- Recent sessions log a single `NARUTO_AUDIO_UNDERRUN` each, and none of them
  covered a cutscene.

## The measurement gap

`AUDIO-02` through `AUDIO-07` have been **PENDING** for a long time for one
reason: no failing cutscene has ever been captured with instrumentation. Every
audio change so far was judged by listening, which is why the balanced downmix
consumed a full cycle before being rejected.

The FPS investigation was unblocked by a single end-to-end metric, simulated
seconds per real second, rather than by inspecting candidate functions. This
plan applies the same principle to audio.

The audio equivalent is **delivered audio seconds per real second, per XMA
context**. A healthy stream delivers 1.0. A starving stream delivers less. A
stream that repeats keeps delivering 1.0 while its read offset returns to the
same position, which distinguishes repetition from starvation immediately.

## Constraints

These are hard requirements, not preferences.

- **No formatting or I/O in the SDL callback.** D016 records that periodic
  logging in the real-time callback was the most plausible source of new
  dropouts and was removed. The callback may only increment counters and
  compute arithmetic on values it already has. All logging happens on another
  thread.
- **Do not repeat rejected experiments.** The balanced stereo downmix is
  rejected (D016, AUDIO-11). Synchronous XMA showed no audible benefit
  (AUDIO-10) and stays a comparison mode, never a fix.
- **Diagnose audio independently** of graphics, guest functions and timing, per
  the engineering rules in `AGENTS.md`.
- The original XEX is never modified.
- Recent work changed presentation pacing and the simulation step. Audio must
  be measured against the current build, and any audio conclusion must be
  checked once with the frame rate mode suspended so a timing interaction
  cannot be mistaken for an audio defect.

## Phase 1 — Instrumentation

One build. No behaviour change; counters and logging only.

**Per-context stream telemetry**, emitted once per second for active contexts
only, from the decoder worker thread:

```text
NARUTO_XMA_STREAM ctx=<n> decoded_ms=<x> wall_ms=<y> fill_ratio=<x/y>
  buffer_swaps=<n> loop_resets=<n> offset_repeats=<n> decode_errors=<n>
  input_valid=<0|1|2> peak=<f>
```

- `fill_ratio` is the primary metric. It is the audio analogue of `raw_speed`.
- `offset_repeats` counts decodes that start from an `input_buffer_read_offset`
  already decoded since the last buffer swap. This is the repetition detector.
- `loop_resets` counts `UpdateLoopStatus` rewinds, separating a legitimate loop
  from a stuck buffer.
- `buffer_swaps` shows whether the guest is feeding the decoder at all.

**Driver-side flow telemetry**, accumulated in the callback and logged from
another thread:

```text
NARUTO_AUDIO_FLOW queued_avg=<n> queued_min=<n> underrun_ms=<x>
  discontinuity_events=<n> max_discontinuity=<f>
```

- `discontinuity_events` counts buffer boundaries where the first sample jumps
  from the previous last sample by more than a threshold. This is a crackle
  detector, and it costs one subtraction per channel per buffer.

**Scene marker.** F7 writes `NARUTO_AUDIO_MARK seq=<n>` to the log. F7 is
currently unbound; F8, F9 and F10 are taken. The marker makes correlation exact
instead of relying on wall-clock estimates.

Exit criterion: a normal session produces the three markers, and a clean boot
plus the 12-trace GPU regression still pass, proving the instrumentation is
inert.

## Phase 2 — Capture and classify

No code changes. Capture only.

1. The reproducible failing cutscene, marked with F7 at its start and end.
2. One representative capture per remaining symptom class.
3. One healthy cutscene as a negative control.
4. One repeat of the reproducible case with the frame rate mode suspended.

Deliverable: a table mapping each observed symptom to its telemetry signature.
The expected discriminations are:

| Symptom | Expected signature |
|---|---|
| Missing | context never activates, or `fill_ratio` near 1.0 with `peak` at zero |
| Cut off | `buffer_swaps` stops while the scene continues |
| Repeating | `offset_repeats` climbs with `fill_ratio` near 1.0 |
| Crackling | `discontinuity_events` climbs, with or without `underrun_ms` |

If the observed signatures do not match this table, the table is wrong and the
model gets revised before any fix is attempted. That sequence is deliberate:
the battle timing work went down a wrong path for two phases because a symptom
was attributed to a cause that had never been measured.

Exit criterion: every reported symptom is assigned to a signature, and the
number of distinct root causes is known rather than assumed.

### Phase 2 result, log `narutobb_112` (2026-07-25)

The reproducible cutscene was captured between two F7 markers, 17.5 s long. The
maintainer reported missing audio, hiss and one repeating fragment, a character
scream, lasting until near the end of the scene.

| Measure | Outside the scene | Inside the scene |
|---|---:|---:|
| Decoded frames | 514/s | 249/s |
| Decode errors | 36.4/s, 6.6% of attempts | **84.3/s, 25.3% of attempts** |
| Offset repeats | 52.6/s | 23.4/s |
| Output queue | 63.5/64 | 63.5/64 |
| Underruns | 4 | **0** |
| Seam discontinuities | 0 | **0** |

The output path is not implicated. The queue stayed effectively full, no
starvation occurred during the scene, and no waveform discontinuity was
measured at any buffer seam. The crackle is therefore not a concealment seam and
the missing audio is not driver starvation.

The one measure that changes magnitude inside the scene is the decode error
rate, which nearly quadruples in share. This revises the plan's own expectation
of separate root causes per symptom: a decoder that loses the packet stream
produces silence, partial frames and a re-read of the same offset, which is
exactly the three reported symptoms from a single cause.

Static reading of the two sites that raise `error_status = 4` found an
asymmetry. When a split frame *header* cannot reach its next packet the code
calls `SwapInputBuffer` and moves on. When a split frame *body* cannot reach its
next packet the code sets the error and returns without swapping and without
advancing `input_buffer_read_offset`, so the next `Work()` call restarts from
the same position. That is a mechanism for the reported repetition, but which
site dominates has not been measured yet, so no fix is written. The error
counter was split into `err_header_split`, `err_next_packet` and the starved
subset of the latter to settle it.

### Phase 3 result, logs `narutobb_113` to `narutobb_116` (2026-07-25)

Splitting the error counter settled the cause. Across a whole session, every
single decode error was the same case, with no exceptions:

| Counter | Outside | Inside the scene |
|---|---:|---:|
| `err_header_split` | 0 (0%) | 0 (0%) |
| `err_next_packet` | 1799 (100%) | 1622 (100%) |
| of which starved | 1799 (100%) | 1622 (100%) |

A frame split across two input buffers could not finish because the guest had
not supplied the following buffer. That is a wait, not a stream error. The
hardware produces no output that tick; the port instead reported
`error_status = 4` and returned without advancing the read offset, so the next
`Work()` restarted from the same position and replayed the same fragment.

Treating the starved case as a stall removed every decode error and raised
decoded frames inside the scene by 23%. The maintainer reported the hiss
largely gone.

An **unbounded** stall was wrong and was corrected. Seven streams were left
waiting forever, one for 21 seconds, because a stream that ends on a split
frame never receives another buffer. The wait is now bounded by
`naruto_xma_stall_timeout_ms`, after which the original error is reported so
the stream terminates. Contexts that end normally show 7 to 12 timeouts over
about 50 seconds, which is the expected shape.

Two pathologies remain, and they are distinct:

- Some contexts cycle several times per second through decode, stall, timeout,
  error and recovery. This is the remaining intermittency.
- Other contexts re-read offsets almost once per decoded frame while barely
  timing out at all. This is the remaining repetition.

The set of affected contexts changes between runs, so this is a condition
streams fall into rather than a defect in a particular stream.

**The frame rate mode is not involved.** The same cutscene captured with the
experiment suspended produced 87.1 stalls and 9.2 timeouts per second against
88.0 and 9.8 with it active, a difference within noise. The recent pacing and
simulation-step work is therefore cleared as a contributing cause.

### The one-buffer deadlock, and a rejected shortcut (2026-07-25)

Two further measurements closed the remaining questions about where the fault
is not, and identified the mechanism.

**Output backpressure is ruled out.** The same cutscene was captured with the
output queue at 128 frames and at 16, an eight-fold change confirmed by
`NARUTO_AUDIO_QUEUE_CONFIG` and by an average depth of 127.5 against 15.5.
Stalls moved by 1%, timeouts by 1%, decoded frames by 0%, with zero underruns
and zero seam discontinuities in both. A different audio backend, XAudio2 or
otherwise, would not address this defect.

**The title keeps one input buffer valid at a time**, in 97% of samples. That
turns the wait into a deadlock: the decoder holds the consumed buffer while
waiting for the next one, and the title cannot supply the next one until the
decoder releases what it holds. Nothing moves until the stall times out,
reports `error_status = 4`, and the title rewinds. Every guest write to the read
offset observed was a rewind, which is the repeated fragment the maintainer
heard.

**Releasing the buffer without retaining the partial frame is rejected.** It
was tried behind `naruto_xma_release_buffer_on_split` and did remove the
deadlock: stalls and timeouts fell to zero for the whole session. It also broke
the game. Contexts left with no valid input buffer rose from 0.3% of samples to
8%, streams died, and the cutscene soft-locked at its end while the title waited
for audio that could no longer arrive. Discarding the leading part of a split
frame desynchronizes the title's accounting of submitted against consumed data.

Partial-frame retention is therefore a requirement of this fix, not a later
refinement. The cvar is disabled by default and its launcher was removed so the
regression cannot be triggered by accident.

## Remaining implementation — retain split frames across a buffer swap

The measurement and root-cause phases are complete for the reproducible scene.
The decoder must retain the leading bytes of a split frame, release the consumed
guest buffer so the title can refill, and resume the same frame when the next
buffer becomes valid. It must not report `error_status = 4`, restart from the
old read offset, or discard data already counted as submitted.

Keep the bounded-stall path as the controlled baseline until retention exists.
The rejected release-without-retention cvar remains disabled and must not gain a
launcher. A fix is not complete until telemetry shows continuous forward
progress and the cutscene exits normally without dead streams or repeated
fragments.

## Phase 4 — Fix and validate

One class per build. Each fix requires:

- before and after captures of the same cutscene, using the same telemetry;
- confirmation that the other symptom classes did not regress;
- a clean build plus the 12-trace GPU regression;
- `AUDIO-02` through `AUDIO-07` updated only for scenarios actually observed,
  never for scenarios inferred from a related pass.

## Risks

- **Multiple causes interacting.** A fix for one class can mask or worsen
  another, which is why classes are separated in phase 2 before any fix.
- **Heisenbugs.** Audio is real-time and instrumentation can change timing. The
  callback path is counter-only for this reason, and phase 1 has an explicit
  inertness check.
- **Symptom vocabulary.** "Crackling" can mean clipping, discontinuity or
  decoder artefacts, which have different causes. The discontinuity counter and
  the existing peak/clipping counters exist to separate them from listening
  impressions.
- **Timing interaction.** The recent pacing and simulation-step changes are new.
  Phase 2 includes a suspended-mode capture specifically to rule them out.
