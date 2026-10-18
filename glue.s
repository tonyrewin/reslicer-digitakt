| SPDX-License-Identifier: GPL-2.0-or-later
| reslicer: beat reslicing for the SLICE machine (beat.c); the glue: the
| machine it adds and the patched SLICE window function (see README.md).
        .ifdef  OS154                   | the Digitakt mk1 1.54 (mod.json's port)
        .include "os154.inc"
        .else                           | the Digitakt mk1 1.53
        .include "os153.inc"
        .endif
        .include "digitakt-mk1/core3.inc"

        .section .run, "ax"

| ---- the RESLICER machine ---------------------------------------------------|
| Machine 5, through core's machine slots (core 2.1, docs/ADAPTING.md "SRC
| machines"): SLICE's parameters and SRC page, and it plays as SLICE (the
| render sees 3), so everything SLICE has works on it. What this mod adds
| is GRID's extra values, the divisions of the beat in two placements:
| stock's range is 0-4 (4, 8, 16, 32, 64 equal slices) and this page runs
| it to 0x1400 (20), so the knob turns past 64 to
|
|   5-12   1/1 1/2 1/4 1/4T 1/8 1/8T 1/16 1/16T   from the sample's start
|  13-20   N1/1 N1/2 ... N1/16 N16T              from the note's start
|
| The N values are the same divisions placed from where the playhead was
| when the note fired, so every note begins on a boundary and keeps its
| place on the beat (beat.c). Their text is beat.c's beat_fmt, and the
| window function below reads the values.
|
| The stock sizes the SLICE knob as 4 << GRID in 8.8, so GRID stops at 20:
| 4 << 20 would be past the knob's own arithmetic. GRID above 20 clamps to
| the last division rather than reading past the table.
|
| The stock SLICE machine is left as it is, so a track that switches to
| ONESHOT or to another machine loses the divisions with the machine: the
| knob keeps the value, which the stock firmware reads as its largest
| grid (64).
        .equ    RS_ID, 5
        .balign 4
        .globl  rs_machine
rs_machine:
        CM_MACHINE RS_ID, str_rs, str_rs_short, rs_icon, 3, 3, rs_page
        .balign 4
rs_page:
        CM_UI   3                                       | SLICE's page
        CM_KNOB                                         | A: TUNE
        CM_KNOB flags=CM_RANGE, min=0, max=0x400, fmt=beat_pfmt
                                                        | B: PLAY, to SHFL
        CM_KNOB                                         | C: BR
        CM_KNOB                                         | D: SAMP
        CM_KNOB                                         | E: SLICE
        CM_KNOB                                         | F: LEN
        CM_KNOB flags=CM_RANGE, min=0, max=0x1400, fmt=beat_fmt
                                                        | G: GRID, to BEAT
        CM_KNOB                                         | H: LEV
| Its menu icon, 11 x 7 as the stock ones (a word a column, rows in bits
| 31-25): SLICE's ramps, with the slice lines on an even beat.
|   #..#....#..
|   #..#....#..
|   ##.##...##.
|   #######.###
|   ##.##...##.
|   #..#....#..
|   #..#....#..
rs_icon:
        .long   BMP_VT, 11, 7, 1, rs_icon_px, rs_icon_mask, 0
rs_icon_px:
        .long   0xfe000000, 0x38000000, 0x10000000, 0xfe000000, 0x38000000
        .long   0x10000000, 0x10000000, 0x00000000, 0xfe000000, 0x38000000
        .long   0x10000000
rs_icon_mask:
        .long   0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000
        .long   0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000
        .long   0xfe000000
str_rs:        .asciz  "RESLICER"
str_rs_short:  .asciz  "RSLC"
        .balign 2

| ---- the slice window: GRID = BEAT (beat.c) ----------------------------------|
| At 0x40074df2 (was: lea -24(sp),sp ; moveq #94,d1), the entry of the
| SLICE window function, by jmp: (sp) = return, 4 p (the voice's
| parameters: PLAY at +2, GRID at +12), 8 note, 12 length, 16 v (the
| voice). A SLICE voice runs the stock code. A RESLICER voice with GRID
| 5 or up, and a tempo measured, goes to beat_win (the same arguments,
| the same d0/d1 result); anything else runs the stock code. The FAST
| AUDIO copy of the block inherits this jmp (it copies the patched
| image), and its fallback runs the image's copy.
|
| The render calls it every block for every SLICE-like voice, not per
| sample: what it costs is a few instructions, and it is the only patch.
        .equ    BEAT0, 5
        .globl  slice_win
slice_win:
        movea.l 4(%sp), %a0
        move.b  12(%a0), %d0           | GRID
        cmpi.b  #BEAT0, %d0
        blt.s   1f
        move.l  bt_spp, %d0            | no step timed yet: the stock grid
        tst.l   %d0
        beq.s   1f
        jmp     beat_win                | tail call: the arguments are ours
1:      lea     -24(%sp), %sp           | the replaced instructions
        moveq   #94, %d1
        jmp     SLICE_WIN_ON
