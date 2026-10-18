/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Digitakt mk1 MAIN OS 1.53: the stock locations beat.c reads (os153.h and
 * os154.h, one per OS; os153.inc includes the one OS154 picks). */
#ifndef RESLICER_OS153_H
#define RESLICER_OS153_H

#define OS_PP_POS 0x8000EDC8u        /* V(v) + 4: a voice's position, V = 0x8000EDC4 + 94 v */
#define OS_PP_TRIG 0x80001228u       /* bit v: a voice v starts */

#endif
