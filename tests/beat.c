/* SPDX-License-Identifier: GPL-2.0-or-later */
/* tests/beat.c: beat.c's arithmetic on the host, where the firmware's
 * addresses and its 48 kHz render do not exist. It includes beat.c itself,
 * so it checks the code that ships, not a copy:
 *
 *     cc -O1 -I . -o /tmp/beat-test tests/beat.c && /tmp/beat-test
 *
 * It covers the division ladder, which slice the voice's parameters pick,
 * LEN's span, PLAY's reverse, the pitch, both grid placements, the tempo
 * meter's accuracy over the Digitakt's range, and the GRID knob's texts.
 * It says nothing about the firmware: for that, build the mod and try it. */
#include <stdio.h>
#include <string.h>
typedef int s32;                    /* beat.c's own typedefs come after */
typedef unsigned int u32;           /* ... and this one too */
static s32 test_pos;               /* the firmware's address has no host */
static u32 test_trig;              /* bit v: a voice starts */
static u32 test_len;
#define VOICE_POS(v) (test_pos)
#define PP_TRIG(v) (test_trig >> (v) & 1)
static u32 test_seed;              /* the shuffle's generator, so a test can
                                     pin it and expect exact rolls */
#define BT_SEED test_seed
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

/* fire a note: the render sets the position after this call, so a test does
 * it by hand, the way the render does */
static void fire(u32 from)
{
    test_trig = 1;
    beat_win(par, 60 << 16, test_len, 0);
    test_trig = 0;
    test_pos = (s32)from;
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
    u32 r, old;
    u64 w;
    int ok = 1;

    /* 120 BPM at 48 kHz: a beat is 24000 samples, a bar 96000, a step
     * (a 1/16 note) 6000. The sample is two bars. */
    bt_spp = 6000;
    test_len = 192000;
    memset(bt_pos, 0, sizeof bt_pos);
    memset(bt_rate, 0, sizeof bt_rate);
    memset(bt_phase, 0, sizeof bt_phase);
    bt_rate[0] = 256;                        /* the voice at its own pitch */

    puts("-- the ladder, from the sample's start, slice 1, two bars at 120 --");
    show("GRID 5  1/1",   192000, 3, 1, 0, 5, 60);
    show("GRID 7  1/2",   192000, 3, 1, 0, 7, 60);
    show("GRID 8  1/2T",  192000, 3, 1, 0, 8, 60);
    show("GRID 9  1/4",   192000, 3, 1, 0, 9, 60);
    show("GRID 11 1/8",   192000, 3, 1, 0, 11, 60);
    show("GRID 12 1/8T",  192000, 3, 1, 0, 12, 60);
    show("GRID 13 ...",   192000, 3, 1, 0, 13, 60);
    show("GRID 19 (N1/8)", 192000, 3, 1, 0, 19, 60);
    show("GRID 20 (N1/16T)", 192000, 3, 1, 0, 20, 60);
    show("GRID 25 past the table", 192000, 3, 1, 0, 25, 60);

    puts("\n-- picking slices at 1/16 (32 slices in two bars) --");
    show("slice 1",       192000, 3, 1,  0, 11, 60);
    show("slice 5",       192000, 3, 5,  0, 11, 60);
    show("slice 32 (last)", 192000, 3, 32, 0, 11, 60);
    show("slice 33 wraps", 192000, 3, 33, 0, 11, 60);
    show("slice 1, LEN 3", 192000, 3, 1,  3, 11, 60);
    show("slice 1, LEN 63", 192000, 3, 1, 63, 11, 60);
    show("slice 0, note 44", 192000, 3, 0, 0, 11, 44);
    show("slice 0, note 45", 192000, 3, 0, 0, 11, 45);
    show("PLAY 1 (REV)",  192000, 1, 1,  0, 11, 60);
    show("half a bar",     48000, 3, 1,  0, 11, 60);
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

    puts("\n-- GRID 13-20: from the note, the phase is where it fired --");
    bt_spp = 6000; bt_rate[0] = 256;
    /* A note fired at sample 3000 (half a 1/16 step in): slice 1 must start
     * at the next boundary, 6000, and slice 2 one step after that. */
    setp(3, 1, 0, 19);                        /* N1/8 */
    test_pos = 3000;
    fire(3000);
    w = beat_win(par, 60 << 16, 192000, 0);
    printf("%s N1/8 fired at 3000: slice 1 at %u..%u (want 6000..12000)\n",
           ((u32)(w >> 32) == 6000 && (u32)w == 12000) ? "ok  " : "FAIL",
           (u32)(w >> 32), (u32)w);
    ok &= (u32)(w >> 32) == 6000 && (u32)w == 12000;

    /* Fired exactly on a boundary: slice 1 starts there. */
    setp(3, 1, 0, 19);
    test_pos = 12000;
    fire(12000);
    w = beat_win(par, 60 << 16, 192000, 0);
    printf("%s N1/8 fired on a boundary: %u..%u (want 12000..18000)\n",
           ((u32)(w >> 32) == 12000 && (u32)w == 18000) ? "ok  " : "FAIL",
           (u32)(w >> 32), (u32)w);
    ok &= (u32)(w >> 32) == 12000 && (u32)w == 18000;

    /* The phase holds for the whole note, and LEN counts steps from it. */
    setp(3, 1, 2, 19);                        /* N1/8, LEN 2 */
    test_pos = 12000;
    fire(12000);
    w = beat_win(par, 60 << 16, 192000, 0);
    printf("%s N1/8 LEN 2 spans three: %u..%u (want 12000..30000)\n",
           ((u32)(w >> 32) == 12000 && (u32)w == 30000) ? "ok  " : "FAIL",
           (u32)(w >> 32), (u32)w);
    ok &= (u32)(w >> 32) == 12000 && (u32)w == 30000;
    /* The next block of the same note must not move the phase. */
    w = beat_win(par, 60 << 16, 192000, 0);
    printf("%s ... and the next block holds it: %u..%u\n",
           ((u32)(w >> 32) == 12000 && (u32)w == 30000) ? "ok  " : "FAIL",
           (u32)(w >> 32), (u32)w);
    ok &= (u32)(w >> 32) == 12000 && (u32)w == 30000;

    /* From the sample (GRID 11) the same note plays from the file's start. */
    setp(3, 5, 0, 11);
    test_pos = 12000;
    w = beat_win(par, 60 << 16, 192000, 0);
    printf("%s 1/16 slice 5 is 24000..30000 whatever fired\n",
           ((u32)(w >> 32) == 24000 && (u32)w == 30000) ? "ok  " : "FAIL");
    ok &= (u32)(w >> 32) == 24000 && (u32)w == 30000;

    puts("\n-- PLAY 4-7: SHFL, rolled once per note --");
    test_pos = 0;
    test_trig = 0;
    /* PLAY 0-3 must be exactly as before: 0 and 1 reverse, 2 and 3 not. */
    {
        u64 fwd, revv;
        setp(3, 1, 0, 11);
        fwd = beat_win(par, 60 << 16, 192000, 0);
        setp(1, 1, 0, 11);
        revv = beat_win(par, 60 << 16, 192000, 0);
        /* d0 is where the voice starts and d1 where it ends: forwards is
         * start < end, backwards is start > end. */
        printf("%s PLAY 3 forwards (0..6000), PLAY 1 backwards (6000..0)\n",
               ((u32)(fwd >> 32) == 0 && (u32)fwd == 6000 &&
                 (u32)(revv >> 32) == 6000 && (u32)revv == 0) ? "ok  " : "FAIL");
        ok &= (u32)(fwd >> 32) == 0 && (u32)fwd == 6000 &&
              (u32)(revv >> 32) == 6000 && (u32)revv == 0;
    }

    /* PLAY 7 (every note): the notes cover the sample, a quarter of them
     * play backwards, and no note ever lands on slice 1 every time. */
    {
        u32 seen[32], i, n = 0, back = 0, note, different = 0;
        memset(seen, 0, sizeof seen);
        memset(bt_pick, 0xff, sizeof bt_pick);
        memset(bt_fresh, 0, sizeof bt_fresh);
        test_seed = 0x12345678u;
        for (note = 0; note < 200; note++) {
            u64 w2;
            test_trig = 1;
            test_pos = 0;
            setp(7, 1, 0, 11);
            w2 = beat_win(par, 60 << 16, 192000, 0);
            test_trig = 0;
            i = (u32)w2 / 6000;                 /* which slice it landed on */
            if (i < 32 && !seen[i])
                seen[i] = 1, n++;
            /* Reversed: d0 holds the slice's end and d1 its start, and a
             * slice's start is always below its end. */
            if ((u32)(w2 >> 32) > (u32)w2)
                back++;
            if (i != 0)
                different++;
        }
        printf("   200 notes at SHFL 7: %u distinct slices, %u backwards, "
               "%u off slice 1\n", n, back, different);
        printf("%s every note is shuffled: many slices, some backwards\n",
               (n > 8 && back > 10 && different > 150) ? "ok  " : "FAIL");
        ok &= n > 8 && back > 10 && different > 150;
    }

    /* PLAY 4 (a quarter): the reverse must land near a quarter of the
     * notes, and most notes must still play the slice asked for. Exactly
     * slice 1 is fewer than three quarters of them, because a quarter of
     * the notes go backwards whatever the roll says about the slice, and
     * a random slice lands back on 1 about one time in 32. */
    {
        u32 note, same = 0, back = 0, other = 0;
        memset(bt_pick, 0xff, sizeof bt_pick);
        memset(bt_fresh, 0, sizeof bt_fresh);
        test_seed = 0xdeadbeefu;
        for (note = 0; note < 400; note++) {
            u64 w2;
            test_trig = 1;
            test_pos = 0;
            setp(4, 1, 0, 11);
            w2 = beat_win(par, 60 << 16, 192000, 0);
            test_trig = 0;
            if ((u32)(w2 >> 32) == 0 && (u32)w2 == 6000)
                same++;
            else if ((u32)(w2 >> 32) > (u32)w2)
                back++;
            else
                other++;
        }
        printf("   400 notes at SHFL 4: %u kept slice 1 forwards, %u backwards, "
               "%u another slice\n", same, back, other);
        printf("%s a quarter reverses about a quarter, and leaves most alone\n",
               (back > 80 && back < 120 && same > 190 && same < 260) ? "ok  " : "FAIL");
        ok &= back > 80 && back < 120 && same > 190 && same < 260;
    }

    /* The roll happens once: the blocks after a note must not change it. */
    {
        u64 a, b, c;
        test_seed = 0x9abcdef0u;
        test_pos = 0;
        memset(bt_pick, 0xff, sizeof bt_pick);   /* -1: the asked-for slice */
        memset(bt_fresh, 0, sizeof bt_fresh);
        test_trig = 1;
        setp(7, 1, 0, 11);
        a = beat_win(par, 60 << 16, 192000, 0);
        test_trig = 0;
        b = beat_win(par, 60 << 16, 192000, 0);
        c = beat_win(par, 60 << 16, 192000, 0);
        printf("%s the same note keeps its slice across blocks (%u..%u)\n",
               (a == b && b == c) ? "ok  " : "FAIL",
               (u32)(a >> 32), (u32)a);
        ok &= a == b && b == c;
    }

    /* PLAY 0-3 must stay deterministic: the generator must not be reached. */
    {
        u64 a, b;
        test_seed = 0x11111111u;
        test_pos = 0;
        memset(bt_pick, 0xff, sizeof bt_pick);
        memset(bt_fresh, 0, sizeof bt_fresh);
        test_trig = 1;
        setp(3, 3, 0, 11);
        a = beat_win(par, 60 << 16, 192000, 0);
        test_trig = 0;
        setp(3, 3, 0, 11);
        b = beat_win(par, 60 << 16, 192000, 0);
        printf("%s the stock directions ignore the generator (slice 3)\n",
               (a == b && test_seed == 0x11111111u) ? "ok  " : "FAIL");
        ok &= a == b && test_seed == 0x11111111u;
    }

    puts("\n-- the PLAY texts --");
    {
        char knob[32], pop[32];
        u32 i;
        for (i = 0; i < 9; i++) {
            memset(knob, 0, sizeof knob);
            memset(pop, 0, sizeof pop);
            if (beat_pfmt(knob, (s32)i * 256, 0, 0)) {
                beat_pfmt(pop, (s32)i * 256, 1, 0);
                printf("PLAY %u  knob %-6s pop-up %s\n", i, knob, pop);
            } else
                printf("PLAY %u  (stock direction)\n", i);
        }
    }

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
    CHECK(0, 6000, "1/16 slice 1", 11, 1, 0, 60);
    CHECK(24000, 30000, "1/16 slice 5", 11, 5, 0, 60);
    CHECK(186000, 192000, "1/16 slice 32 ends the sample", 11, 32, 0, 60);
    CHECK(0, 6000, "1/16 slice 33 wraps to 1", 11, 33, 0, 60);
    CHECK(0, 24000, "LEN 3 spans four 1/16 slices", 11, 1, 3, 60);
    CHECK(0, 192000, "LEN 63 stops at the sample's end", 11, 1, 63, 60);
    CHECK(0, 6000, "SLICE 0, note 44 (44-12 = 32, one loop)", 11, 0, 0, 44);
    CHECK(6000, 12000, "SLICE 0, note 45 (33, slice 2)", 11, 0, 0, 45);
    setp(3, 1, 0, 11);
    w = beat_win(par, 60 << 16, 192000, 9);
    if ((u32)(w >> 32) == 0 && (u32)w == 192000)
        printf("ok   a voice outside 0..7 gets the whole sample\n");
    else {
        printf("FAIL a voice outside 0..7: got %u..%u\n", (u32)(w >> 32), (u32)w);
        ok = 0;
    }
    setp(1, 1, 0, 11);
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
        for (i = 0; i < 26; i++) {
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
