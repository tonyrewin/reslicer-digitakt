| SPDX-License-Identifier: GPL-2.0-or-later
| Digitakt mk1 MAIN OS 1.53: the stock locations glue.s reads (os153.inc and
| os154.inc, one per OS).
        | ---- the sites' continuations, and what glue.s uses --------------------
        .equ BMP_VT,       0x401b73b4    | the firmware's Bitmap vtable
        .equ SLICE_WIN_ON, 0x40074df8    | the SLICE window function, past its site
