/* SPDX-License-Identifier: GPL-2.0-or-later */
/* beat.c: beat reslicing for the SLICE machine (GRID = BEAT).
 *
 * Stock SLICE (machine type 3) divides a sample into 4, 8, 16, 32 or 64
 * equal slices: GRID 0-4. This mod gives GRID 5 and up, on the DIGISLICER
 * machine, the divisions of the beat instead: a slice is one step of the
 * pattern apart from the next, so the sample is resliced as 1/8, 1/16,
 * 1/8T and so on at the project's tempo, and the slices keep their place
 * when the tempo changes.
 *
 * Nothing is stored, nothing is analysed, no sample is managed and no
 * memory is taken. The SLICE window function at 0x40074df2, hooked by
 * glue.s, is handed the voice and does the arithmetic: the slice starts
 * are k * (a step in samples). The only thing measured is the tempo, from
 * the sequencer's own clock, and the voice's speed, from its position.
 */
#ifdef OS154                       /* the Digitakt mk1 1.54 (mod.json's port) */
#include "os154.h"
#else                              /* the Digitakt mk1 1.53 */
#include "os153.h"
#endif

typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef unsigned long long u64;

/* ---- the divisions -------------------------------------------------------- */
/* GRID 5 and up: one slice per step of the pattern (a step is a 1/16
 * note), or per two, four ... of them, or per a triplet. The table is
 * steps per division in 1/256 of a step: a 1/8 is two steps, 512. */
#define BEAT0   5                     /* the first GRID value that is a beat */
#define NDIV    14
static const u16 div_step[NDIV] = {
    4096, 2731, 2048, 1365, 1024, 683, 512,
    341, 256, 171, 128, 85, 64, 43
};
/* the names, six bytes each so that one offset indexes them */
#define DIV_NAME(i) (div_name + 6 * (i))
static const char div_name[6 * NDIV] =
    "1/1\0\0\0"   "1/1T\0\0"  "1/2\0\0\0"   "1/2T\0\0"
    "1/4\0\0\0"   "1/4T\0\0"  "1/8\0\0\0"   "1/8T\0\0"
    "1/16\0\0"    "1/16T\0"   "1/32\0\0"    "1/32T\0"
    "1/64\0\0"    "1/64T\0";

/* ---- the tempo: samples a step takes -------------------------------------- */
/* The UI task ticks 30 times a second and the render runs at 48 kHz, so a
 * tick is 1600 samples. ev_fire_step (seq-events) counts the steps the
 * sequencer fires, one call each, exactly; beat_tick counts the ticks, and
 * a window of ticks with the steps in it gives the samples a step takes.
|
 * Both windows are closed by the ticks and never by a step count: closing
 * on a step count would put the window's span on a step's edge, and the
 * tick it landed on would then be the same every window, a tempo's worth
 * of error every time. Closed by the ticks, a window is as long as the
 * counter says, and the steps inside it are counted exactly.
|
 * What is left is that a window holds a whole number of steps and a
 * fraction of one, so the count is one step out either way. A window of a
 * hundred steps is a percent, of forty steps 2.5%. So a window's length
 * wanders by up to sixteen ticks (DITHER), which keeps the steps' phase
 * against the ticks from being the same twice running and turns that
 * rounding from a bias into something the average takes away, and a
 * window only speaks once it holds FAST_MIN steps, whose one step is
 * narrower than the change it is read for.
|
 * The fast window is for following the user: a tempo a fortieth from the
 * last is one they have moved, and is taken at once. The slow one is the
 * value itself, averaged in as each of its windows closes. Below about 90
 * BPM the fast window never holds that many steps, and the tempo is then
 * the slow window's to find, every thirty seconds. */
#define TICK_SAMPLES 1600
#define SPAN_FAST    300             /* ten seconds */
#define SPAN_SLOW    900             /* thirty seconds */
#define DITHER       16              /* how far a window's length wanders */
#define FAST_MIN     64              /* steps the fast window needs to speak */
#define SLOW_MIN     3               /* and the slow one */
#define MOVE_PCT     40              /* this far off (1/40) is a new tempo */

volatile u32 bt_spp;                /* samples a step takes, 0 until measured */

static u32 bt_fires, bt_ticks;      /* the fast window */
static u32 bt_sfires, bt_sticks;    /* the slow one */
static u32 bt_dither;               /* walks its way through 0..32 */

/* In the sequencer's step fire, at interrupt level: count the steps of the
 * first audio track, which run at the pattern's tempo. The step it hands
 * back is the step to read, which is the one it read. */
s32 beat_fire_step(s32 track, s32 step, s32 len)
{
    if (track == 0) {
        bt_fires++;
        bt_sfires++;
    }
    return step;
}

/* A window: its ticks, and the steps counted in it. How many samples a
 * step takes, or 0 while it is still open or too rough to read. */
static u32 window(u32 *ticks, u32 *fires, u32 span, u32 min)
{
    u32 t = ++*ticks, n = *fires, spp;

    if (n < min || t < span)
        return 0;
    *ticks = *fires = 0;
    spp = t * TICK_SAMPLES / n;
    return spp >= TICK_SAMPLES ? spp : 0;    /* a step under a tick cannot be */
}

/* The UI task's 30 Hz tick: time the steps the windows have counted.
 * While the sequencer is stopped no step is counted and both stand. */
void beat_tick(void)
{
    u32 f, s, v, d;

    bt_dither = bt_dither * 37 + 11;
    d = bt_dither % (2 * DITHER + 1);
    f = window(&bt_ticks, &bt_fires, SPAN_FAST + d, FAST_MIN);
    s = window(&bt_sticks, &bt_sfires, SPAN_SLOW + d, SLOW_MIN);

    v = f ? f : s;                  /* the fast window first: it needs more
                                     * steps to speak, so it is the finer of
                                     * the two */
    if (!v)
        return;
    if (!bt_spp)
        bt_spp = v;                  /* the first window to speak sets it */
    else if ((d = v > bt_spp ? v - bt_spp : bt_spp - v) > bt_spp / MOVE_PCT)
        bt_spp = v;                  /* the tempo moved: take this one */
    else if (s)
        bt_spp = (bt_spp * 3 + s) / 4;
}

/* ---- the window ------------------------------------------------------------ */
/* The slice window of the SLICE machine, called from the hook in glue.s
 * with the arguments of 0x40074df2 (sp = return, 4 p = the voice's
 * parameters: PLAY at +2, SLICE at +8, LEN at +10, GRID at +12, 8 note =
 * the trig's note << 16, 12 len = the sample's length, 16 v = the voice).
 * Returns d0 = where the voice starts, d1 = where it ends, as the stock:
 * slice k = SLICE - 1 (wrapped), or the note (- 12, wrapped) when SLICE is
 * 0; LEN more slices after it; reversed for PLAY 0 and 1. */
#ifndef VOICE_POS                 /* a host test (tests/beat.c) replaces it */
#define VOICE_POS(v) (*(volatile s32 *)(OS_PP_POS + 94 * (v)))
#endif

static u32 bt_pos[8];              /* the voice's position when it was here */
static u16 bt_rate[8];             /* the frames it eats a sample, 1/256 */

u64 beat_win(const s8 *p, u32 note, u32 len, s32 v)
{
    u32 g = (u8)p[12], i, step, n, from, to, pos, d, r;
    s32 k, sel = (u8)p[8], L = (u8)p[10];

    if (v < 0 || v > 7)
        return len;                 /* not one of the voices: all of it */

    /* The voice's speed: the frames it eats between two blocks, over the
     * 32 samples a block holds, which is its rate at its own pitch (256).
     * The render sets the position after this call, so the first block of
     * a note keeps the last rate: one block is under a millisecond. */
    pos = VOICE_POS(v);
    d = pos - bt_pos[v];
    bt_pos[v] = pos;
    if (d > 16 && d < 256) {        /* half a block to eight: its own rate
                                    * is 256, and a jump or a stall is out */
        r = d * 8;
        bt_rate[v] = bt_rate[v] ? (bt_rate[v] * 3 + r) / 4 : r;
    }

    /* A division in samples: a step, times the table, times the voice's
     * rate, so a pitched sample's slices are a beat long as played. */
    i = g - BEAT0;
    if (i >= NDIV)
        i = NDIV - 1;
    step = bt_spp * div_step[i] >> 8;
    if (bt_rate[v])
        step = step * bt_rate[v] >> 8;
    if (!step)
        step = 1;

    /* The slices the sample holds at that division, and the one to play. */
    n = (len + step - 1) / step;
    if (!n)
        n = 1;
    if (sel > 0)
        k = (sel - 1) % (s32)n;
    else {
        k = ((s32)(s16)(note >> 16) - 12) % (s32)n;
        if (k < 0)
            k += n;
    }
    from = (u32)k * step;
    to = (u32)(k + L + 1) < n ? (u32)(k + L + 1) * step : len;
    if (to > len)
        to = len;
    if (to <= from)
        to = from + 1;

    if ((u8)p[2] > 1)               /* PLAY 0 and 1 play it backwards */
        return ((u64)from << 32) | to;
    return ((u64)to << 32) | from;
}

/* The GRID knob's text, from the page glue.s describes: a division's name,
 * or 0 for the stock text (4, 8, 16, 32, 64). value is 8.8. */
s32 beat_fmt(char *buf, s32 value, s32 ctx, s32 machine)
{
    const char *s;
    u32 i = (u32)value >> 8, n;

    (void)ctx;
    (void)machine;
    if (i < BEAT0)
        return 0;
    i -= BEAT0;
    if (i >= NDIV)
        i = NDIV - 1;
    s = DIV_NAME(i);
    for (n = 0; n < 5 && s[n]; n++)
        buf[n] = s[n];
    buf[n] = 0;
    return 1;
}
