// cp_vt.h 
//
// Copyright (c) 2006, Mike Acton <macton@cellperformance.com>
//
// Modified for compilation on SPU by Jonathan Adamczewski <jonathan@brnz.org>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without
// limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
// EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef CP_VT_H
#define CP_VT_H

#if defined(__cplusplus)
extern "C" 
{
#endif

typedef struct cp_vt cp_vt;

struct cp_vt
{
    int tty_ndx; 	 // used as DMA target - aligned(16)
    int prev_tty_ndx;
    int tty;
	int pad;
    int prev_kdmode; // used as DMA target - aligned(16)
} __attribute__((aligned(16)));

int cp_vt_open_graphics(struct cp_vt* __restrict vt, void* space);
int cp_vt_close(struct cp_vt* __restrict vt);

#if defined(__cplusplus)
}
#endif

#endif
