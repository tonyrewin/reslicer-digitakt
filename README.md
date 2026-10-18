# reslicer

**Beat reslicing for the Digitakt (mk1).** SLICE cuts a sample into 4, 8,
16, 32 or 64 equal slices. reslicer gives the same knob the divisions of
the beat instead: **1/1, 1/2, 1/4, 1/8, 1/8T, 1/16, 1/32T** and down to
1/64T, at the project's tempo. A slice is one step of the pattern apart
from the next, so a loop is resliced as it is heard: the slices keep
their place when the tempo changes, and a pitched sample's slices are a
beat long *as played*.

It is an [elekloader](https://github.com/irpina/elekloader) mod for OS
1.53 and 1.54, a file for each. elekloader builds a custom OS file on
your own machine, from your stock OS file and the mods you pick; nothing
from Elektron is distributed.

**It is not a slicer.** Nothing is analysed, nothing is stored, no
slices are kept on the +Drive, no waveform is drawn and no sample is
managed: the mod is the machine's own slice window with different
arithmetic. A track on the stock SLICE machine is untouched.

## What it does

- **The RESLICER machine.** Pick it as any machine: FUNC + SRC, then
  RESLICER, below SLICE, in the list. Its SRC page is SLICE's and it
  plays as SLICE, so everything SLICE has works on it: TUNE, PLAY, BR,
  SAMP, SLICE, LEN, LEVEL, slice locks, the keyboard, recording.
- **GRID turns past 64 into the beat.** On this machine the GRID knob
  runs from 4, 8, 16, 32, 64 (the stock values) to `1/1`, `1/1T`, `1/2`,
  `1/2T`, `1/4`, `1/4T`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`,
  `1/32T`, `1/64`, `1/64T`. Whatever the division, the sample is cut into
  slices exactly that long, and the slice number (the SLICE knob, the
  note when SLICE is 0, the LEN knob's span) picks among them as it does
  on the stock machine.
- **The tempo is the project's, measured as it plays.** The mod counts
  the steps the sequencer fires and times them against the UI task's
  30 Hz tick, which is 1600 samples at 48 kHz: a division is that many
  samples per step times the division. Change the tempo and the grid
  follows; stop the sequencer and the grid keeps the last tempo.
  - **How close:** the clock is the UI tick, so the grid is within about
    a percent of the project's beat, a little more below 50 BPM. It takes
    up to half a minute of playing to settle from cold, and a tempo move
    is followed within ten seconds above 90 BPM, every thirty seconds
    below it.
- **A pitched sample stays on the beat.** The voice's position says how
  fast it plays, so the grid is scaled by its own rate: at +12 semitones a
  1/8 slice is twice as many frames of the sample, and still an eighth
  note as played.
- **The stock SLICE machine is left as it is.** A track that switches to
  ONESHOT or to another machine keeps its GRID value, which the stock
  firmware reads as its largest grid (64).
- **It leaves room for other mods.** Of the 128 KB of RAM that
  elekloader's mods share, reslicer takes 72 bytes, and 1.5 KB of the
  firmware's own code space (934 in `beat.c`, 540 in `glue.s`, measured
  with `m68k-elf-gcc -Os`; the built `.elemod` says `.run` 1356,
  `.bss` 72). A firmware with reslicer, core, machine-pages and
  seq-events links at 18 740 of 262 144 bytes of RAM, 0 of 2304 of fast
  SRAM. Version 2.x, the slice editor, took 15 KB.

## Install

You need three things:
- **elekloader**:
  - **Windows:** download `elekloader-<version>-windows.zip` from
    [elekloader's releases](https://github.com/irpina/elekloader/releases/latest),
    unzip it and run `elekloader.exe`. The core mod, which every linkable mod
    needs, is built in.
  - **Other systems:** run elekloader from source with Python 3.9 or newer
    (see [its README](https://github.com/irpina/elekloader#install)). There
    you also need **core 3.0** for your OS (`core-3.0.elemod` for 1.53,
    `core-3.0-os1.54.elemod` for 1.54) from
    [elekloader's releases](https://github.com/irpina/elekloader/releases),
    plus **machine-pages** and **seq-events**. reslicer needs all three:
    core for the machine slots, machine-pages to draw the page and extend
    the GRID knob past 64, seq-events for the step counter.
- **This mod**, from
  [this repository's releases](https://github.com/tonyrewin/reslicer/releases/latest):
  `reslicer-3.0.elemod` for OS 1.53, `reslicer-3.0-os1.54.elemod` for
  OS 1.54. They are the same mod.
- **The stock OS file** your unit runs: `Digitakt_OS1.54.syx` or
  `Digitakt_OS1.53.syx`, from
  [Elektron's Digitakt downloads](https://www.elektron.se/support-downloads/digitakt).
  elekloader recognises it by its hash, and refuses a mod file made for
  the other OS.

Then build your OS in elekloader's window:

1. **Change stock firmware...** (top right): choose your stock OS file.
2. **+ Install from file...**: choose the reslicer file for that OS, and
   core, machine-pages and seq-events the same way. From source, install
   core the same way.
3. **Tick reslicer.** Its three requirements are ticked with it. The
   check below the list should say "No conflicts ... Ready to build". To
   add [digihealth](https://github.com/irpina/digihealth) (FAST AUDIO and
   SYSTEM INFO), install and tick it as well.
4. **OS version shown**: the 4 characters the unit will show, for example
   `DTS3`.
5. **BUILD FIRMWARE**, and save the `.syx`. elekloader verifies it before
   writing it.

Flash it with Elektron Transfer, as for any OS update
([Elektron's instructions](https://support.elektron.se/support/solutions/articles/43000662890-how-to-update-your-device)):
1. Connect the unit over USB.
2. In Transfer, select the unit and **Connect**.
3. Drag the `.syx` onto **Drop files here**.
4. Press **YES** on the unit.

Don't turn it off until the upgrade is done.

## Build it from source

The Digitakt mk1 cross toolchain (m68k binutils and gcc; on Debian or
Ubuntu, `apt install binutils-m68k-linux-gnu gcc-m68k-linux-gnu`; on
Windows, inside WSL) and elekloader:

```bash
python -m elekloader.sdk.build . --stock Digitakt_OS1.53.syx       # -> out/reslicer-3.0.elemod
python -m elekloader.sdk.build . --stock Digitakt_OS1.54.syx       # -> out/reslicer-3.0-os1.54.elemod
python -m elekloader.lint out/reslicer-3.0-os1.54.elemod --stock Digitakt_OS1.54.syx \
    --with core.elemod --with machine-pages.elemod --with seq-events-dt1.elemod
```

The dependencies are mods in elekloader's own source (`mods/core`,
`mods/machine-pages`, `mods/seq-events-dt1`), each built by the same
command. The firmware itself is `elekloader.patch`:

```bash
python -m elekloader.patch --stock Digitakt_OS1.54.syx \
    --mod core-4.0-os1.54.elemod --mod machine-pages-1.2-os1.54.elemod \
    --mod reslicer-3.0-os1.54.elemod --mod seq-events-0.1-os1.54.elemod \
    --out reslicer-3.0-os1.54.syx --version RSL3
```

On macOS with Homebrew's toolchain (`brew install m68k-elf-gcc`, which
names its tools `m68k-elf-as` and not `m68k-linux-gnu-as`), point
elekloader at it:

```bash
export ELEKLOADER_CROSS=m68k-elf-
```

To measure the mod without building it (`m68k-elf-gcc`, or `m68k-linux-gnu-gcc`):

```bash
m68k-elf-gcc -Os -c beat.c -o beat.o && m68k-elf-size -A beat.o
m68k-elf-gcc -Os -I <elekloader>/sdk/include -c glue.s -o glue.o && m68k-elf-size -A glue.o
```

| file | |
|---|---|
| `mod.json` | the mod: its one patched site, its handlers, its machine, what it requires |
| `beat.c` | the divisions, the tempo measurement, the slice window |
| `glue.s` | the RESLICER machine (its core descriptor, icon and page: GRID's range to 1/64T) and the patched SLICE window function |
| `os153.inc`, `os154.inc` | the stock locations `glue.s` uses, for each OS (1.54's port defines `OS154`) |
| `os153.h`, `os154.h` | the same for `beat.c` |
| `tests/beat.c` | `beat.c`'s arithmetic on the host: no firmware, no device |

## How it was checked

`tests/beat.c` includes `beat.c` itself and runs it on the host, where the
firmware's addresses and its render do not exist:

```bash
cc -O1 -I . -o /tmp/beat-test tests/beat.c && /tmp/beat-test
```

It covers the division ladder against the beat, which slice the voice's
SLICE, LEN, PLAY and note pick, the last slice and the wrap, a pitched
voice, the tempo meter's accuracy over the Digitakt's tempo range, and
every GRID text. It says nothing about the firmware: for that the mod has
to be built with [elekloader](https://github.com/irpina/elekloader) and
played, which has not been done for 3.0.

## How it works

Stock SLICE's window function, at `0x40074df2`, is what turns a note into
the range of the sample it plays. It builds its grid when the sample is
loaded: 256 positions per sample slot, and GRID says which of the 4, 8,
16, 32 or 64 cells of that grid to take. That function is the only thing
this mod patches.

On a RESLICER voice whose GRID is 5 or up, and once a tempo has been
measured, the call goes to `beat_win` instead, with the same arguments
and the same result. It reads the voice's GRID, SLICE, LEN, PLAY and
note, works out

    a division = (samples a step takes) x (the division in steps) / 256

and returns `k x a division` to `(k + LEN + 1) x a division`, wrapped to
the sample's length, the last slice running to its end, reversed for
PLAY's REV modes. Nothing is stored, so a different sample, division or
tempo is one multiply away, and the render costs the same for all of
them.

The tempo comes out of two counters: `ev_fire_step` (seq-events) counts
the steps the sequencer fires, one call each, exactly, and `ev_tick` (the
UI task's 30 Hz tick, 1600 samples at 48 kHz) times them. A window is
closed by the ticks and never by a step count, because a window closed on
a step would land on the same tick every window and keep its rounding
(± one step) as a bias; a window's length also wanders by up to sixteen
ticks, so the steps' phase against the ticks is not the same twice
running and the rounding averages away. A ten second window with its
sixty-four steps is the one that follows the user: a tempo a fortieth
from the last is taken at once. The thirty second window is the value
itself. What is left is the tick's own resolution, about one part in a
hundred.

Before the first window closes, `bt_spp` is 0 and the patch runs the
stock window, so a build that never plays a step behaves exactly as the
stock firmware does.

The voice's speed is read the same way, from its position (the render
sets it after the window, so a new note's first block keeps the last
rate): the frames it eats between two blocks over the 32 samples a block
holds is its rate at its own pitch, which scales the grid.

## From 2.x

2.x, called digislicer, was a slicer: it analysed transients (GRID =
AUTO), kept per-sample slice points in a store on the +Drive (`/cfw/
slices.a`, `.b`), had a waveform editor with zoom, menus and audition,
added a ping-pong loop (PLAY = PIPO) and a trig slice mode for the
keyboard. All of that is gone in 3.0: it is 1.5 KB of firmware instead
of 15 KB, and it does one thing.

A project saved with 2.x still loads: it keeps its RESLICER tracks, and
they play. The stock SLICE machine was never touched by either version,
and a 2.x RESLICER track's own slices play as stock slices do (its GRID
reads as 64). Nothing is lost but the feature itself: `AUTO` is the
stock equal grid now, `PIPO` is stock `FWD.L`, and the editor and the
+Drive folder are gone with the files `/cfw/slices.a`, `/cfw/slices.b`.

## Licence

GPL-2.0-or-later, as [elekloader](https://github.com/irpina/elekloader)
mods are. See `LICENSE`.
