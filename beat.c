/* SPDX-License-Identifier: GPL-2.0-or-later */
/* beat.c: beat reslicing for the SLICE machine (GRID = BEAT).
 *
 * Stock SLICE (machine type 3) divides a sample into 4, 8, 16, 32 or 64
 * equal slices: GRID 0-4. This mod gives GRID 5 and up, on the RESLICER
 * machine, the divisions of the beat instead: a slice is one step of the
 * pattern apart from the next, so the sample is resliced as 1/8, 1/16,
 * 1/8T and so on at the project's tempo, and the slices keep their place
 * when the tempo changes.
 *
 * Two placements of that grid, both on GRID, chosen by the range:
 *   - GRID 5-12 (SAMPLE): from the start of the file, as the stock grid
 *     is. Slice k is always the same hit, whatever note asks for it.
 *   - GRID 13-20 (NOTE): from where the voice's playhead was when the
 *     note fired, so every note starts on a slice boundary and keeps its
 *     place on the beat even when the sample's start is not the downbeat
 *     or its length is not a whole number of bars. The same slice number
 *     then walks the sample as the pattern plays.
 *
 * PLAY's fifth value (SHFL) adds SupaTrigga's rearrange and reverse: with
 * a set chance a note plays a random slice instead of the one it asked
 * for, and plays it backwards. Both are decided once, when the note
 * fires, so the pattern stays musical instead of dissolving into
 * sample-level noise: the window comes every block, 1500 a second, and a
 * decision each time would be a decision per 32 samples. It needs no
 * patch of the render, which reads PLAY 4 as it reads FWD.
 *
 * Nothing is stored, nothing is analysed, no sample is managed and no
 * memory is taken. The SLICE window function at 0x40074df2, hooked by
 * glue.s, is handed the voice and does the arithmetic. The only things
 * measured are the tempo, from the sequencer's own clock, and the voice's
 * speed, from its position.
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
 * note), or per two or four of them, or per a triplet. The table is steps
 * per division in 1/256 of a step: a 1/8 is two steps, 512.
|
 * The ladder stops at 1/16T because the stock sizes the SLICE knob as
 * 4 << GRID in 8.8: 4 << 12 is 64.0, 4 << 13 would be its sign bit. */
#define BEAT0   5                     /* the first GRID value that is a beat */
#define NOTE0   13                    /* the first NOTE-placed division */
#define NDIV    8
static const u16 div_step[NDIV] = {
    4096, 2048, 1024, 683, 512, 341, 256, 171
};
/* The knob names, six bytes each so one offset indexes them: SAMPLE keeps
 * the division ("1/8"), NOTE marks it with a leading N ("N1/8"). The
 * popup shows the same; the README says what N means. */
#define DIV_NAME(i) (div_name + 6 * (i))
static const char div_name[6 * NDIV] =
    "1/1\0\0\0" "1/2\0\0\0" "1/4\0\0\0" "1/4T\0\0"
    "1/8\0\0\0" "1/8T\0\0" "1/16\0\0" "1/16T\0";
static const char div_name_note[6 * NDIV] =
    "N1/1\0\0" "N1/2\0\0" "N1/4\0\0" "N1/4T\0"
    "N1/8\0\0" "N1/8T\0"  "N1/16\0"  "N16T\0";

/* ---- PLAY's fifth value: SHFL -------------------------------------------- */
/* Stock PLAY is 0 REV, 1 REV.L, 2 FWD.L, 3 FWD; 4 and up are ours. The
 * render reads PLAY for its direction (0 and 1 backwards) and its loop
 * flag ((PLAY - 1) < 2), and for 4 and up both come out as they do for
 * FWD, so nothing has to be rewritten for the render to play them
 * forwards and not to loop them.
|
 * SHFL takes the chance, in quarters, and SupaTrigga's two effects:
 *   4  a quarter   5  a half   6  three quarters   7  every note
| Each is rolled on its own when a note fires, so a quarter-chance mode
| gives a quarter of the notes a random slice and a quarter of them
| backwards, and the two coincide only where both come up. */
#define SHFL0   4                     /* PLAY's first SHFL value */
#define NSHFL  4                     /* how many */
static const u8 shfl_pct[NSHFL] = { 64, 128, 192, 255 };

/* A xorshift, seeded from something that differs between boots. It only
 * has to not give the same pattern every time the sequencer starts. The
 * name is a macro so a host test (tests/beat.c) can pin it and expect
 * exact rolls. */
static u32 bt_seed;
#ifndef BT_SEED
#define BT_SEED bt_seed
#endif

static u32 bt_rand(void)
{
    u32 x = BT_SEED;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (!x)                          /* xorshift cannot leave zero, and zero
                                     * would be the same pattern for ever */
        x = 0x9e3779b9u;
    BT_SEED = x;
    return x;
}

/* ---- the tempo: samples a step takes -------------------------------------- */
/* The UI task ticks 30 times a second and the render runs at 48 kHz, so a
 * tick is 1600 samples. ev_fire_step (seq-events) counts the steps the
 * sequencer fires, one call each, exactly; beat_tick counts the ticks, and
 * a window of ticks with the steps in it gives the samples a step takes.
|
 * Both windows are closed by the ticks and never by a step count: closing
 * on a step count would put the window's span on a step's edge, and
 * the tick it landed on would then be the same every window, a tempo's
 * worth of error every time. Closed by the ticks, a window is as long as
 * the counter says, and the steps inside it are counted exactly.
|
 * What is left is that a window holds a whole number of steps and a
 * fraction of one, so the count is one step out either way. A window of a
 * hundred steps is a percent, of forty steps 2.5%. So a window's length
 * therefore wanders by up to sixteen ticks (DITHER), which keeps the
 * steps' phase against the ticks from being the same twice running and
 * turns that rounding from a bias into something the average takes away,
 * and a window only speaks once it holds FAST_MIN steps, whose one step
 * is narrower than the change it is read for.
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
#ifndef PP_TRIG                  /* bit v: a voice v starts (tests too) */
#define PP_TRIG(v) (*(volatile u32 *)OS_PP_TRIG >> (v) & 1)
#endif

static u32 bt_pos[8];              /* the voice's position when it was here */
static u16 bt_rate[8];             /* the frames it eats a sample, 1/256 */
static u32 bt_phase[8];            /* NOTE mode: where slice 0 was placed */
static s16 bt_pick[8];             /* SHFL: the slice this note plays, -1
                                       while the note keeps the one its
                                       SLICE knob and note asked for */
static u8 bt_fresh[8];             /* SHFL: and whether it plays backwards */

u64 beat_win(const s8 *p, u32 note, u32 len, s32 v)
{
    u32 g = (u8)p[12], i, step, n, from, to, pos, d, r;
    s32 k, sel = (u8)p[8], L = (u8)p[10], note_mode, rev;
    u8 play = (u8)p[2];

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

    note_mode = g >= NOTE0;

    /* A division in samples: a step, times the table, times the voice's
     * rate, so a pitched sample's slices are a beat long as played. */
    i = (u32)(note_mode ? g - NOTE0 : g - BEAT0);
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

    /* SHFL: roll once, when the note fires, not once per block. Both
     * rolls take the same chance. What came up is latched, because the
     * window comes again every block and the note has to keep the slice
     * and the direction it started with: a fresh roll each block would
     * be a fresh roll per 32 samples.
     *
     * The reverse is the window's order, which is how the stock plays a
     * slice backwards. The render is not told and does not need to be: it
     * reads PLAY as it reads FWD, and the order in is the order out. */
    rev = 0;
    if (play >= SHFL0) {
        i = play - SHFL0;
        if (i >= NSHFL)
            i = NSHFL - 1;
        if (PP_TRIG(v)) {
            u32 pct = shfl_pct[i];
            u32 a = bt_rand();
            bt_pick[v] = -1;
            bt_fresh[v] = 0;
            if ((a & 0xff) < pct)
                bt_pick[v] = (s16)(bt_rand() % (u32)n);
            if (((a >> 8) & 0xff) < pct)
                bt_fresh[v] = 1;
        }
        if (bt_pick[v] >= 0)
            k = bt_pick[v];
        rev = bt_fresh[v];
    } else if (play <= 1)            /* stock REV and REV.L */
        rev = 1;

    /* NOTE mode: the grid's zero is the first boundary at or after where
     * the playhead stood when the note fired, so the note begins on a
     * slice and each slice after it is one step (or one 1/8, two steps)
     * later. The render sets the position after this call, so on the block
     * a trig bit is set the position still reads where the last note
     * ended: that is the input this wants. Latched, so it holds for the
     * whole note and moves on the next trig.
     *
     * The boundary is pos rounded up to the next multiple of step, and
     * pos itself when it is already on one. */
    if (note_mode && step < len) {
        if (PP_TRIG(v))
            bt_phase[v] = (pos + step - 1) / step * step;
        from = bt_phase[v] + (u32)k * step;
        if (from >= len)
            from -= len;            /* phase is under a step, k is under n */
    } else {
        from = (u32)k * step;
    }
    to = from + (u32)(L + 1) * step;
    if (to > len)
        to = len;
    if (to <= from)
        to = from + 1;

    /* The order out is the order the stock plays in: forwards is d0 =
     * where it starts, d1 = where it ends; reversed swaps them. rev holds
     * what PLAY says and what SHFL rolled. */
    if (rev)
        return ((u64)to << 32) | from;
    return ((u64)from << 32) | to;
}

/* The PLAY knob's text, from the page glue.s describes: SHFL and its
 * chance for values 4 and up, 0 for the stock names (REV, REV.L, FWD.L,
 * FWD). value is 8.8. */
static const char *const shfl_name[NSHFL] = {
    "SHFL", "SHFL", "SHFL", "SHFL"
};
static const char *const shfl_long[NSHFL] = {
    "SHUFFLE 25%", "SHUFFLE 50%", "SHUFFLE 75%", "SHUFFLE EVERY"
};

s32 beat_pfmt(char *buf, s32 value, s32 ctx, s32 machine)
{
    const char *s;
    u32 i = (u32)value >> 8, n;

    (void)machine;
    if (i < SHFL0)
        return 0;                   /* the stock's four directions */
    i -= SHFL0;
    if (i >= NSHFL)
        i = NSHFL - 1;
    s = ctx ? shfl_long[i] : shfl_name[i];    /* ctx 1 is the pop-up */
    for (n = 0; n < 15 && s[n]; n++)
        buf[n] = s[n];
    buf[n] = 0;
    return 1;
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
    if (i >= NOTE0) {
        i -= NOTE0;
        s = div_name_note + 6 * (i >= NDIV ? NDIV - 1 : i);
    } else {
        i -= BEAT0;
        s = div_name + 6 * (i >= NDIV ? NDIV - 1 : i);
    }
    for (n = 0; n < 5 && s[n]; n++)
        buf[n] = s[n];
    buf[n] = 0;
    return 1;
}
