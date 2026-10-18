/* SPDX-License-Identifier: GPL-2.0-or-later */
/* tests/beat.c: beat.c's arithmetic on the host, where the firmware's
 * addresses and its 48 kHz render do not exist. It includes beat.c itself,
 * so it checks the code that ships, not a copy:
 *
 *     cc -O1 -I . -o /tmp/beat-test tests/beat.c && /tmp/beat-test
 *
 * It covers the division ladder, which slice the voice's parameters pick,
 * LEN's span, PLAY's reverse, the pitch and the tempo, the tempo meter's
 * accuracy over the Digitakt's range, and the GRID knob's texts. It says
 * nothing about the firmware: for that, build the mod and try it. */
#include <stdio.h>
#include <string.h>
typedef int s32;                    /* beat.c's own typedefs come after */
static s32 test_pos;               /* the firmware's address has no host */
#define VOICE_POS(v) (test_pos)
#include "../beat.c"

static s8 par[16];
static void setp(int play, int slice, int len, int grid)
{
    memset(par, 0, sizeof par);
    par[2] = (s8)play;
    par[8] = (s8)slice;
    par[10] = (s8)len;
    par[12] = (s8)grid;
}

static void show(const char *what, u32 len, int play, int slice, int l, int grid, int note)
{
    u64 w;
    u32 from, to;

    setp(play, slice, l, grid);
    w = beat_win(par, (u32)note << 16, len, 0);
    from = (u32)(w >> 32);
    to = (u32)w;
    printf("%-32s GRID=%-2d SLICE=%-2d LEN=%-2d note=%-3d -> %7u..%-7u (%u)\n",
           what, grid, slice, l, note, from, to, to - from);
}

/* feed the tempo meter a tempo for `ticks` ticks, 30 a second */
static void meter(double bpm, int ticks)
{
    double acc = 0.0, step = 15.0 / bpm;    /* a step is a 1/16 note */
    int i, n = 0;

    for (i = 0; i < ticks; i++) {
        acc += 1.0 / 30.0;
        while (acc >= step) {
            beat_fire_step(0, n++ & 63, 64);
            acc -= step;
        }
        beat_tick();
    }
}

int main(void)
{
    u32 old, r;
    u64 w;
    int ok = 1;

    /* 120 BPM at 48 kHz: a beat is 24000 samples, a bar 96000, a step
     * (a 1/16 note) 6000. The sample is two bars. */
    bt_spp = 6000;
    memset(bt_pos, 0, sizeof bt_pos);
    memset(bt_rate, 0, sizeof bt_rate);
    bt_rate[0] = 256;                        /* the voice at its own pitch */

    puts("-- the ladder, one slice from the start, two bars at 120 BPM --");
    show("GRID 5  1/1",   192000, 3, 1, 0, 5, 60);
    show("GRID 7  1/2",   192000, 3, 1, 0, 7, 60);
    show("GRID 8  1/2T",  192000, 3, 1, 0, 8, 60);
    show("GRID 9  1/4",   192000, 3, 1, 0, 9, 60);
    show("GRID 11 1/8",   192000, 3, 1, 0, 11, 60);
    show("GRID 12 1/8T",  192000, 3, 1, 0, 12, 60);
    show("GRID 13 1/16",  192000, 3, 1, 0, 13, 60);
    show("GRID 15 1/32",  192000, 3, 1, 0, 15, 60);
    show("GRID 17 1/64",  192000, 3, 1, 0, 17, 60);
    show("GRID 20 past 1/64T", 192000, 3, 1, 0, 20, 60);

    puts("\n-- picking slices at 1/16 (32 slices in two bars) --");
    show("slice 1",       192000, 3, 1,  0, 13, 60);
    show("slice 5",       192000, 3, 5,  0, 13, 60);
    show("slice 32 (last)", 192000, 3, 32, 0, 13, 60);
    show("slice 33 wraps", 192000, 3, 33, 0, 13, 60);
    show("slice 1, LEN 3", 192000, 3, 1,  3, 13, 60);
    show("slice 1, LEN 63", 192000, 3, 1, 63, 13, 60);
    show("slice 0, note 44", 192000, 3, 0, 0, 13, 44);
    show("slice 0, note 45", 192000, 3, 0, 0, 13, 45);
    show("PLAY 1 (REV)",  192000, 1, 1,  0, 13, 60);
    show("half a bar",     48000, 3, 1,  0, 13, 60);
    show("1/1 on one beat", 24000, 3, 1,  0, 5,  60);

    puts("\n-- the tempo and the pitch move the grid --");
    old = bt_spp; bt_spp = 720000 / 140;      /* 140 BPM */
    show("1/8 at 140 BPM", 192000, 3, 1, 0, 11, 60);
    bt_spp = old;
    r = bt_rate[0]; bt_rate[0] = 512;
    show("1/8, voice at 2x", 192000, 3, 1, 0, 11, 60);
    bt_rate[0] = 128;
    show("1/8, voice at 0.5x", 192000, 3, 1, 0, 11, 60);
    bt_rate[0] = r;

    puts("\n-- the checks --");
    bt_spp = 6000; bt_rate[0] = 256;
#define CHECK(want_from, want_to, what, grid, slice, len, note)                    \
    do {                                                                           \
        setp(3, slice, len, grid);                                                 \
        w = beat_win(par, (u32)(note) << 16, 192000, 0);                           \
        if ((u32)(w >> 32) == (want_from) && (u32)w == (want_to))                  \
            printf("ok   %s\n", what);                                            \
        else {                                                                     \
            printf("FAIL %s: got %u..%u, want %u..%u\n", what, (u32)(w >> 32),     \
                   (u32)w, (u32)(want_from), (u32)(want_to));                     \
            ok = 0;                                                                \
        }                                                                          \
    } while (0)
    CHECK(0, 96000, "1/1 is two bars at 120 BPM", 5, 1, 0, 60);
    CHECK(0, 6000, "1/16 slice 1", 13, 1, 0, 60);
    CHECK(24000, 30000, "1/16 slice 5", 13, 5, 0, 60);
    CHECK(186000, 192000, "1/16 slice 32 ends the sample", 13, 32, 0, 60);
    CHECK(0, 6000, "1/16 slice 33 wraps to 1", 13, 33, 0, 60);
    CHECK(0, 24000, "LEN 3 spans four 1/16 slices", 13, 1, 3, 60);
    CHECK(0, 192000, "LEN 63 stops at the sample's end", 13, 1, 63, 60);
    CHECK(0, 6000, "SLICE 0, note 44 (44-12 = 32, one loop)", 13, 0, 0, 44);
    CHECK(6000, 12000, "SLICE 0, note 45 (33, slice 2)", 13, 0, 0, 45);
    setp(3, 1, 0, 13);
    w = beat_win(par, 60 << 16, 192000, 9);
    if ((u32)(w >> 32) == 0 && (u32)w == 192000)
        printf("ok   a voice outside 0..7 gets the whole sample\n");
    else {
        printf("FAIL a voice outside 0..7: got %u..%u\n", (u32)(w >> 32), (u32)w);
        ok = 0;
    }
    setp(1, 1, 0, 13);
    w = beat_win(par, 60 << 16, 192000, 0);
    if ((u32)(w >> 32) == 6000 && (u32)w == 0)
        printf("ok   PLAY 1 returns the window backwards\n");
    else {
        printf("FAIL PLAY 1: got %u..%u\n", (u32)(w >> 32), (u32)w);
        ok = 0;
    }

    puts("\n-- the tempo meter (the UI tick is the clock, so about a percent) --");
    bt_spp = 0; bt_fires = bt_ticks = bt_sfires = bt_sticks = bt_dither = 0;
    meter(120.0, 30 * 40);
    printf("120 BPM: %u samples a step (6000 is exact, %+.2f%%)\n",
           bt_spp, 100.0 * ((double)bt_spp / 6000.0 - 1.0));
    ok &= bt_spp > 5910 && bt_spp < 6090;
    meter(140.0, 30 * 40);
    printf("140 BPM: %u samples a step (5143 is exact, %+.2f%%)\n",
           bt_spp, 100.0 * ((double)bt_spp / 5143.0 - 1.0));
    ok &= bt_spp > 5068 && bt_spp < 5218;
    meter(90.0, 30 * 40);
    printf("90 BPM:  %u samples a step (8000 is exact, %+.2f%%)\n",
           bt_spp, 100.0 * ((double)bt_spp / 8000.0 - 1.0));
    ok &= bt_spp > 7880 && bt_spp < 8120;
    meter(20.0, 30 * 60);
    printf("20 BPM:  %u samples a step (36000 is exact, %+.2f%%)\n",
           bt_spp, 100.0 * ((double)bt_spp / 36000.0 - 1.0));
    ok &= bt_spp > 35100 && bt_spp < 36900;

    /* the sequencer stopped: the value stands */
    old = bt_spp;
    beat_tick();
    beat_tick();
    if (bt_spp == old)
        printf("ok   a stopped sequencer keeps the tempo\n");
    else {
        printf("FAIL a stopped sequencer changed the tempo\n");
        ok = 0;
    }

    puts("\n-- the GRID texts --");
    {
        char buf[32];
        u32 i;
        for (i = 0; i < 20; i++) {
            memset(buf, 0, sizeof buf);
            if (beat_fmt(buf, i << 8, 0, 0))
                printf("GRID %-2u %s\n", i, buf);
            else
                printf("GRID %-2u (stock: %d)\n", i, 4 << (i > 4 ? 4 : i));
        }
    }

    printf("\n%s\n", ok ? "all checks passed" : "SOMETHING FAILED");
    return !ok;
}
