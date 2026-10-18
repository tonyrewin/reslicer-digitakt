| SPDX-License-Identifier: GPL-2.0-or-later
| Digitakt mk1 MAIN OS 1.54: the stock locations glue.s reads.
        | ---- the sites' continuations, and what glue.s uses --------------------
        .equ BMP_VT,       0x401b7734    | the firmware's Bitmap vtable
        .equ SLICE_WIN_ON, 0x40074df8    | the SLICE window function, past its site
