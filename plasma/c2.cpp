/*
 * Diamond-square 'plasma' generator.
 *
 * Work in progress.  Runs at 1080p/60fps from a single SPU.
 *
 * Jonathan Adamczewski, 2010
 *
 * jonathan@brnz.org
 *
 */


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern "C"
{
#include <sys/mman.h>
}

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "shuffle_generator.h"

#include "cp_vt.h"
#include "cp_fb.h"


// Tile width, including sundry extras
static const unsigned int T = 145;
// qword scaled
static const unsigned int T4 = (T+3)/4;

// Space for two tiles (for double buffering)
static qword a[2][T][T4];

// A few constants for access to tile
// index of last item
static const unsigned int L = T - 1; // 144
// index of postive offset of pixels in tile
static const unsigned int P = 8;
// index of negative offset of pixels in tile
static const unsigned int N = L - P; // 136
// Number of pixels
static const unsigned int S = 128;

//#define TRACK_TICKS

// Tool to measure time of various sections.  Accurate and not too intrusive.
//
#ifdef TRACK_TICKS
// Storage for recorded time offsets - stored and written later to avoid 
// time overhead of printf()
static unsigned int tick_store[1024];
static unsigned int next_tick = 0;
#endif

// record passage of time since last tick
static void tick() {
#ifdef TRACK_TICKS
	static unsigned unsigned int last = -1;
	unsigned int t = spu_read_decrementer();
	tick_store[next_tick++] = last - t;
	last = t;
#endif
}

static void tick_print_all() {
#ifdef TRACK_TICKS
	for(int i = 0; i < next_tick; ++i) {
		printf("%d ", tick_store[i]);
	}
	printf("\n");
#endif
}

static void tick_reset() {
#ifdef TRACK_TICKS
	next_tick = 0;
#endif
}

// storage for seed values for tiles and perturbation
const int seed_x = 17;
const int seed_y = 12;
const int seed_size_qw = (seed_y * seed_x) >> 2;

// make it easier to access in more than one way
union seed_space {
	int  i[seed_x][seed_y];
	qword q[seed_size_qw];
} seed;

const float pi = 3.14159265f;


// generate initial tile corners for entire frame
static void genseed() {
	for(int x = 0; x < 17; ++x) {
		for(int y = 0; y < 12; ++y) {
			seed.i[x][y] =  (int)((sinf(x*pi/3.f) + cos(y*pi/3.f) + 2.f) * 64.f);
			seed.i[x][y] |= (int)((sinf(x*pi/6.f+pi/3) + cos(y*pi/6.f+pi/3) + 2.f) * 64.f)<<8;
			seed.i[x][y] |= (int)((sinf(x*pi/2.f+pi/5) + cos(y*pi/2.f+pi/5) + 2.f) * 64.f)<<16;
			printf("%x ", seed.i[x][y]);
		}
		printf("\n");
	}
}

// vary initial tile corners over time
static void perturbseed() {
	static float i = 1.0001f;
	for(int x = 0; x < 17; ++x) {
		for(int y = 0; y < 12; ++y) {
			seed.i[x][y] =  (int)((sinf((x+3.f*i)*pi/3.f) + cos((y-9.f*i)*pi/3.f) + 2.f) * 64.f);
			seed.i[x][y] |= (int)((sinf((x-0.1f*i)*pi/6.f+pi/3.f) + cos((y+i*0.25f)*pi/6.f+pi/3) + 2.f) * 64.f)<<8;
			seed.i[x][y] |= (int)((sinf((x+i)*pi/2.f+pi/5.f) + cos((y-0.7f*i)*pi/2.f+pi/5) + 2.f) * 64.f)<<16;
		}
	}
	i+=0.01f;
}

// returns a qword full of not-very-random based on distance
// the goal is to make this consistent between calls to ensure tile edges match
// Should be extened (I think) to take into account x&y positions, which would improve randomness.
qword random(int distance) {
	unsigned char c = distance-1;
	qword qc = {c,c,c,c, c,c,c,c, c,c,c,c, c,c,c,c};
	qword r = si_and(seed.q[distance%seed_size_qw], qc);
	return r;
}

// loads seed into qword
static qword readseed(int x, int y) {
	qword s = si_from_int(seed.i[x][y]);
	return si_shufb(s, s, SHUF4(A,0,0,0));
}

// applies seed values to all 16 corners needed for tile
static void applyseed(int b, int x, int y) {
	a[b][0][0] =    readseed(x-1,y-1);
	a[b][0][P>>2] = readseed(x-1,y);
	a[b][0][N>>2] = readseed(x-1,y+1);
	a[b][0][L>>2] = readseed(x-1,y+2);

	a[b][P][0] =    readseed(x,y-1);
	a[b][P][P>>2] = readseed(x,y);
	a[b][P][N>>2] = readseed(x,y+1);
	a[b][P][L>>2] = readseed(x,y+2);

	a[b][N][0] =    readseed(x+1,y-1);
	a[b][N][P>>2] = readseed(x+1,y);
	a[b][N][N>>2] = readseed(x+1,y+1);
	a[b][N][L>>2] = readseed(x+1,y+2);

	a[b][L][0] =    readseed(x+2,y-1);
	a[b][L][P>>2] = readseed(x+2,y);
	a[b][L][N>>2] = readseed(x+2,y+1);
	a[b][L][L>>2] = readseed(x+2,y+2);
}


// calculate average of four pixels
qword avg4(qword a, qword b, qword c, qword d) __attribute__((always_inline));
qword avg4(qword a, qword b, qword c, qword d) {
	const qword c2 = si_ilh(0x202);

    // shift each right by 2 bits, masking shifted-in bits from the result
    qword au = si_andbi(si_rotqmbii(a, -2), 0x3f);
    qword bu = si_andbi(si_rotqmbii(b, -2), 0x3f);
    qword cu = si_andbi(si_rotqmbii(c, -2), 0x3f);
    qword du = si_andbi(si_rotqmbii(d, -2), 0x3f);

    // add them all up
    qword R = si_a(si_a(au,bu), si_a(cu,du));

    // add up the lower bits
    qword L = si_a(si_a(si_andbi(a,3),si_andbi(b,3)), si_a(si_andbi(c,3),si_andbi(d,3)));

	// add two for rounding
	L = si_ah(L, c2);

    // shift right 2 bits, again masking out shifted-in high bits
    R = si_a(R, si_andbi(si_rotqmbii(L,-2), 3));

    return R;
}

/* interpolation functions for squares and diamonds.
 *
 * For squares, points are:
 *     (x0,y0)--(x0,y1)
 *        |        |
 *        |        |
 *     (x1,y0)--(x1,y1)
 *
 *  For diamonds, points are
 *         (x0,y1)
 *          /   \
 *         /     \
 *     (x1,y0) (x1,y2)
 *         \     /
 *          \   /
 *         (x2,y1)
 */

// interpolate four corners of a square, where the pixel position is the same in each qword
static qword i4a4(int b, int x0, int y0, int x1, int y1) {
	assert(y0%4==0);
	assert(y1%4==0);
	int y04 = y0 >> 2;
	int y14 = y1 >> 2;
	return avg4(a[b][x0][y04],
				a[b][x0][y14],
				a[b][x1][y04],
				a[b][x1][y14]);
}

// interpolate four corners of a diamond, where the pixel position is the same in each qword
static qword i4d4(int b, int x0, int x1, int x2, int y0, int y1, int y2) {
	assert(y0%4==0);
	assert(y1%4==0);
	assert(y2%4==0);
	int y04 = y0 >> 2;
	int y14 = y1 >> 2;
	int y24 = y2 >> 2;
	return avg4(a[b][x1][y04],
				a[b][x0][y14],
				a[b][x2][y14],
				a[b][x1][y24]);
}

// perform assignment of aligned square interpolate, inserting result correctly into qword
static void pla4(int b, int x, int y, int x0, int y0, int x1, int y1) {
	assert(y0%4==0);
	assert(y1%4==0);
	assert(y%4==0);
	int i = y>>2;
	a[b][x][i] = si_shufb(i4a4(b, x0,y0,x1,y1), a[b][x][i], SHUF4(A,b,c,d));
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[b][x][y]);
}

// perform assignment of offset square interpolate, inserting result into qword
static void pla2(int b, int x, int y, int x0, int y0, int x1, int y1) {
	assert(y0%4==0);
	assert(y1%4==0);
	assert((y+2)%4==0);
	int i = y>>2;
	a[b][x][i] = si_shufb(i4a4(b, x0,y0,x1,y1), a[b][x][i], SHUF4(a,b,A,d));
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[x][y]);
}

// perform assignment of aligned diamond interpolate, inserting result into qword
static void pda4(int b, int x, int y, int x0, int x1, int x2, int y0, int y1, int y2) {
	assert(y0%4==0);
	assert(y1%4==0);
	assert(y2%4==0);
	assert(y%4==0);
	int i = y>>2;
	a[b][x][i] = /*si_xor(*/si_shufb(i4d4(b, x0,x1,x2,y0,y1,y2), a[b][x][i], SHUF4(A,b,c,d))/*, random(x-x0))*/;
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[b][x][y]);
}


// interpolate four points with symmetrical offsets, but not consistent qword offsets
// i.e. x0 = m, x1 = n, y0 = m, y1 = n
static qword i4s(int b, int m, int n) {
	qword mo = si_from_int((m&3)<<2);
	qword no = si_from_int((n&3)<<2);
	int m4 = m >> 2;
	int n4 = n >> 2;
	return avg4(si_rotqby(a[b][m][m4], mo),
				si_rotqby(a[b][m][n4], no),
				si_rotqby(a[b][n][m4], mo),
				si_rotqby(a[b][n][n4], no));
}

// interpolate four corners of a square where pixel position may not be the same in each qword
static qword i4a(int b, int x0, int y0, int x1, int y1) {
	qword y0o = si_from_int((y0&3)<<2);
	qword y1o = si_from_int((y1&3)<<2);
	int y04 = y0 >> 2;
	int y14 = y1 >> 2;
	return avg4(si_rotqby(a[b][x0][y04], y0o),
				si_rotqby(a[b][x0][y14], y1o),
				si_rotqby(a[b][x1][y04], y0o),
				si_rotqby(a[b][x1][y14], y1o));
}

// interpolate four corners of a diamond where pixel position may not be the same in each qword
static qword i4d(int b, int x0, int x1, int x2, int y0, int y1, int y2) {
	qword y0o = si_from_int((y0&3)<<2);
	qword y1o = si_from_int((y1&3)<<2);
	qword y2o = si_from_int((y2&3)<<2);
	int y04 = y0 >> 2;
	int y14 = y1 >> 2;
	int y24 = y2 >> 2;
	return avg4(si_rotqby(a[b][x1][y04], y0o),
				si_rotqby(a[b][x0][y14], y1o),
				si_rotqby(a[b][x2][y14], y1o),
				si_rotqby(a[b][x1][y24], y2o));
}

// perform assignment of unaligned symmetrical square interpolate, inserting result into qword
static void pls(int b, int x, int y, int u, int v) {
	a[b][x][y>>2] = si_shufb(i4s(b,u,v), a[b][x][y>>2], si_cwx(si_from_int(y<<2), si_from_ptr(a[b][x])));
//	printf("%2x %2x %2x %2x       %2x\n", x, y, b, c, a[b][x][y]);
}

// perform assigment of unaligned square interpolate, inserting result into qword
static void pla(int b, int x, int y, int x0, int y0, int x1, int y1) {
	a[b][x][y>>2] = si_shufb(i4a(b, x0,y0,x1,y1), a[b][x][y>>2], si_cwx(si_from_int(y<<2), si_from_ptr(a[b][x])));
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[b][x][y]);
}

// perform assignment of unaligned diamond interpolate, inserting result into qword
static void pda(int b, int x, int y, int x0, int x1, int x2, int y0, int y1, int y2) {
	a[b][x][y>>2] = si_shufb(i4d(b, x0,x1,x2,y0,y1,y2), a[b][x][y>>2], si_cwx(si_from_int(y<<2), si_from_ptr(a[b][x])));
}

/*
// perform assignment of unaligned diamond interpolate, inserting result into word position 2 of qword
static void pdao2(int b, int x, int y, int x0, int x1, int x2, int y0, int y1, int y2) {
	a[b][x][y>>2] = si_shufb(i4d(b, x0,x1,x2,y0,y1,y2), a[b][x][y>>2], SHUF4(a,b,A,d));
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[b][x][y]);
}
*/

// perform assignment of unaligned diamond interpolate, inserting result into word position 0 of qword
static void pdao4(int b, int x, int y, int x0, int x1, int x2, int y0, int y1, int y2) {
	assert(y%4==0);
	a[b][x][y>>2] = si_shufb(i4d(b, x0,x1,x2,y0,y1,y2), a[b][x][y>>2], SHUF4(A,b,c,d));
//	printf("%2x %2x %2x %2x %2x %2x %2x\n", x, y, x0, y0, x1, y1, a[b][x][y]);
}

#ifdef DEBUG
// Print the contents of a qword 
static void pq(qword q) {
	union {
		qword q;
		unsigned char c[16];
	} u;
	u.q = q;
	printf("{");
	for(int i = 0; i < 15; ++i) {
		printf(" 0x%x,", u.c[i]);
	}
	printf(" 0x%x }\n", u.c[15]);
}


// print the tile as text in a vaguely useful fashion
static void pr(int b, int i, int r) {

	printf("-- %d %d --\n    ", i, r);
	for(int k = 0; k < T; k+=1) {
		printf("%d", k%10);
	}
	printf("\n");
	for(int j = 0; j < (T); j+=1) {
		printf("%3d ", j);
		for(int k = 0; k < T; k+=1) {
			unsigned int o = si_to_char(si_rotqby(a[b][j][k>>2],si_from_int((k%4)*4)));
			printf("%c", o==0?' ':'0'+o);
		}
		printf("\n");
	}

}
#endif

/* Pixel calculation.
 *
 * The tile is stored with an inner portion of the generated pixel data with 
 * extra space around the outside to store points needed for the computation
 * but that are not part of the finished product.  This outer storage is
 * compressed to require a much smaller amount of space, and so access to
 * those points requires special care.
 *
 * For each stage of interpolation, four stages take place:
 *  - square interpolation of the outer points  - sqouter()
 *      Points that depend on the outer (compressed) data are calculated,
 *      being four lines of points around the outside.
 *  - square interpolation of the inner points  - sqinner()
 *      Only accessing the pixel data of the tile, there is no accessing of
 *      the compressed outer points
 *  - diamond interpolation of the outer points - diouter()
 *  - diamond interpolation of the inner points - diinner()
 *      Each of these depends on the square interpolation and has a more
 *      complex offsetting.
 *
 * Each of these functions is parameterised by the buffer (b) and the size
 * of the square/diamond (r).  Outer calculations also have an arg (i) for
 * the relevant index into the outer section.
 *
 * Functions are also specialised for r=4 and r=2 cases
 *
 */

static void sqouter(int b, int i, int r) {
	int r2 = r >> 1;
	// corner
	// top left
	pls(b, i+1,i+1,i,P);
	// top right
	pla(b, L-i-1,i+1, N,i,L-i,P);
	// bottom left
	pla(b, i+1,L-i-1, i,N,P,L-i);
	// bottom right
	pls(b, L-i-1,L-i-1, L-i,N);

	// remaining sides
	for(unsigned int u = P; u < (S+P); u += r) {
		// top
		pla(b, u+r2,i+1, u,i,u+r,P);
		// left
		pla(b, i+1,u+r2, i,u,P,u+r);
		// right
		pla(b, L-i-1,u+r2, L-i,u,N,u+r);
		// bottom
		pla(b, u+r2,L-i-1, u,L-i,u+r,N);
	}
}

static void sqinner(int b, int r) {
	// 2d loops to interpolate squares
	int r2 = r >> 1;
	for(unsigned int u = P; u < (S+P); u += r) {
		for(unsigned int v = P; v < (S+P); v += r) {
			pla4(b, u+r2,v+r2, u,v,u+r,v+r);
		}
	}
}

static void diouter(int b, int i, int r) {
	int r2 = r >> 1;

	// inside corners
	// top left
	pda(b,   P,i+1, i+1,P,P+r2,  i,i+1,P);
	pdao4(b, i+1,P,  i,i+1,P,   i+1,P,P+r2);
	// bottom left
	pdao4(b, i+1,N,   i,i+1,P,   N-r2,N,L-i-1);
	pda(b,  P,L-i-1, i+1,P,P+r2, N,L-i-1,L-i);
	// top right
	pda(b,    N,i+1,  N-r2,N,L-i-1, i,i+1,P);
	pdao4(b, L-i-1,P, N,L-i-1,L-i, i+1,P,P+r2);
	// top left lower
	pda(b,   N,L-i-1, N-r2,N,L-i-1, N,L-i-1,L-i);
	pdao4(b, L-i-1,N, N,L-i-1,L-i,  N-r2,N,L-i-1);

	for(unsigned int u = P; u < (S+P); u += r) {
		// for each r, add one point on each side
		// top
		pdao4(b, u+r2,P, u,u+r2,u+r,i+1,P,P+r2);
		// left
		pdao4(b, P,u+r2, i+1,P,P+r2,u,u+r2,u+r);
		// right
		pdao4(b, N,u+r2, N-r2,N,L-i-1,u,u+r2,u+r);
		// bottom
		pdao4(b, u+r2,N, u,u+r2,u+r,N-r2,N,L-i-1);
	}

	//fill in the further out points - needs special case...
	for(unsigned int u = P+r; u < (S+P); u += r) {
		//top
		pda(b, u,i+1, u-r2,u,u+r2, i,i+1,P);
		//left
		pdao4(b, i+1,u, i,i+1,P, u-r2,u,u+r2);
		//right
		pdao4(b, L-i-1,u, N,L-i-1,L-i, u-r2,u,u+r2);
		//bottom
		pda(b, u,L-i-1, u-r2,u,u+r2, N,L-i-1,L-i);
	}

}

static void diinner(int b, int r) {
	// 2d loops to interpolate diamonds
	int r2 = r >> 1;
	for(unsigned int u = (r+P); u < (S+P); u += r) {
		for(unsigned int v = (r2+P); v < (S+P); v += r) {
			pda4(b, u,v, u-r2,u,u+r2,v-r2,v,v+r2);
			pda4(b, v,u, v-r2,v,v+r2,u-r2,u,u+r2);
		}
	}
}


// diouter() and diinner() specialised for r=4
static void diall4(int b) {
	int i = 5;
	// inside corners
	// top left
	pda(b,   P,i+1, i+1,P,P+2,  i,i+1,P);
	pdao4(b, i+1,P,  i,i+1,P,   i+1,P,P+2);
	// bottom left
	pdao4(b, i+1,N,   i,i+1,P,   N-2,N,L-i-1);
	pda(b,  P,L-i-1, i+1,P,P+2, N,L-i-1,L-i);
	// top right
	pda(b,    N,i+1,  N-2,N,L-i-1, i,i+1,P);
	pdao4(b, L-i-1,P, N,L-i-1,L-i, i+1,P,P+2);
	// top left lower
	pda(b,   N,L-i-1, N-2,N,L-i-1, N,L-i-1,L-i);
	pdao4(b, L-i-1,N, N,L-i-1,L-i,  N-2,N,L-i-1);

	//fill in the further out points - needs special case...
	for(unsigned int u = P+4; u < (S+P); u += 4) {
		//top
		pda(b, u,i+1, u-2,u,u+2, i,i+1,P);
		//left
		pdao4(b, i+1,u, i,i+1,P, u-2,u,u+2);
		//right
		pdao4(b, L-i-1,u, N,L-i-1,L-i, u-2,u,u+2);
		//bottom
		pda(b, u,L-i-1, u-2,u,u+2, N,L-i-1,L-i);
	}
				tick();

	// very time consuming loop (~20% of total runtime)
	// Plenty of scope to speed it up - a good start would be to
	// work in the same direction (rather than perpendicular, as
	// implemented).
	for(unsigned int u = P; u < (S+P+4); u += 4) {
		for(unsigned int v = (P+2); v < (S+P); v += 4) {
			qword r;
			int yv04 = (v-2) >> 2;
			int yv14 = yv04; //v >> 2;
			int yv24 = (v+2) >> 2;
			r = avg4(a[b][u][yv04],
					 si_rotqbyi(a[b][u-2][yv14], 8),
					 si_rotqbyi(a[b][u+2][yv14], 8),
					 a[b][u][yv24]);
//			r = si_xor(r, random(4));
			a[b][u][yv14] = si_shufb(r, a[b][u][yv14], SHUF4(a,b,A,d));

			int yu04 = (u-2) >> 2;
			int yu14 = u >> 2;
			int yu24 = yu14; //(u+2) >> 2;
			r = avg4(si_rotqbyi(a[b][v][yu04], 8),
						a[b][v-2][yu14],
						a[b][v+2][yu14],
						si_rotqbyi(a[b][v][yu24], 8));
//			r = si_xor(r, random(4));
			a[b][v][yu14] = si_shufb(r, a[b][v][yu14], SHUF4(A,b,c,d));
		}
	}
				tick();
}

// sqinner() specialised for r=4
static void sqinner4(int b) {
	for(unsigned int u = P; u < (S+P); u += 4) {
		for(unsigned int v = P; v < (S+P); v += 4) {
			pla2(b, u+2,v+2, u,v,u+4,v+4);
		}
	}
}


/* squinner2() - squinner() specialised for r = 2
 *
 * This function calculates four averages per iteration.
 *
 * Six qwords are loaded from three rows, named as left and right for 
 * the row, like so:
 *
 *  u  : [l0][r0]
 *  u+2: [l2][r2]
 *  u+4: [l4][r4]
 *
 * These are shuffled so that the right values are in place for a call to avg4()
 *  M0 = {l0[2], 0, r0[0], 0}
 *  M2 = {l2[2], 0, r2[0], 0}
 *  M4 = {l4[2], 0, r4[0], 0}
 *
 * avg4() calls will then calculate the averages of
 *  {l0[0], l2[0], l0[2], l2[2]} and {l0[2], l2[2], r0[0], r2[0]}
 *    and
 *  {l2[0], l4[0], l2[2], l4[2]} and {l2[2], l4[2], r2[0], r4[0]}
 *
 *  i.e. two meaningful results per call to avg4()
 *
 *  These are then shuffled beck into the tile data 
 *  (effectively, into qwords l1 and l3)
 *
 *  Loop has been unrolled to save a few loads, could be unrolled further.
 */
static void sqinner2(int b) {
	for(unsigned int u = P; u < (S+P); u += 4) {
		qword r0 = a[b][u][P>>2];
		qword r2 = a[b][u+2][P>>2];
		qword r4 = a[b][u+4][P>>2];
		for(unsigned int v = (P>>2); v < ((S+P)>>2); v += 1) {
			unsigned int x0 = u;
			unsigned int y0 = v;
			// load 4 qwords
			qword l0 = r0;//a[x0]  [y0];
			qword l2 = r2;//a[x0+2][y0];
			qword l4 = r4;//a[x0+4][y0];
			r0 = a[b][x0]  [y0+1];
			r2 = a[b][x0+2][y0+1];
			r4 = a[b][x0+4][y0+1];
			// merge
			qword s_C0a0 = SHUF4(C,0,a,0);
			// M0 = {l0[2], 0, r0[0], 0}
			qword M0 = si_shufb(l0, r0, s_C0a0);
			// M2 = {l2[2], 0, r2[0], 0}
			qword M2 = si_shufb(l2, r2, s_C0a0);
 			// M4 = {l4[2], 0, r4[0], 0}
			qword M4 = si_shufb(l4, r4, s_C0a0);
			// average
			qword a1 = avg4(l0, l2, M0, M2);
//			a1 = si_xor(a1, random(2));
			qword a3 = avg4(l2, l4, M2, M4);
//			a3 = si_xor(a3, random(2));
			// write back
			a[b][x0+1][y0] = si_shufb(a[b][x0+1][y0], a1, SHUF4(A,a,C,c));
			a[b][x0+3][y0] = si_shufb(a[b][x0+3][y0], a3, SHUF4(A,a,C,c));
		}
	}
}


/* diall2() - diouter() and diinner() combined and specialised for r=2
 *
 * diouter() and diiner() may be combined as value of i and i+1 mean that
 * no special case is required.
 *
 * Calculates eight averages per iteration.
 *
 * Ten qwords are loaded, labelled with l, c, and r for left, centre and right,
 * with a digit indicating row, like so:
 *
 * u-1:     [c0]
 * u  :     [c1][r1]
 * u+1: [l2][c2]
 * u+2:     [c3][r3]
 * u+3: [l4][c4]
 * u+4:     [c5]
 *
 * Being all the points required to load eight diamonds.
 *
 * These are then rotated and shuffled to put the necessary pixels into word
 * position 0 or 2 for avg4().
 *
 * The calls to avg4() will average the points
 * {c0[1], c1[0], c2[1], c1[2]} and {c0[3], c1[2], c2[3], r1[0]}
 * {c1[0], c2[1], c3[0], l2[3]} and {c1[2], c2[3], c3[2], c1[1]}
 *
 * and likewise for the other two calls.
 *
 * The results are shuffled back into qwords c1, c2, c3, and c4 and written
 * to the tile.
 *
 */
static void diall2(int b) {
	for(unsigned int u = P; u < S+P; u += 4) {
		for(unsigned int v = P >> 2; v < ((S+P)>>2); v += 1) {
			// load 6+4 qwords
			qword l2 = a[b][u+1][v-1];
			qword l4 = a[b][u+3][v-1];
			qword c0 = a[b][u-1][v];
			qword c1 = a[b][u]  [v];
			qword c2 = a[b][u+1][v];
			qword c3 = a[b][u+2][v];
			qword c4 = a[b][u+3][v];
			qword c5 = a[b][u+4][v];
			qword r1 = a[b][u]  [v+1];
			qword r3 = a[b][u+2][v+1];
			// rotate even rows one to the left
			qword c0L = si_rotqbyi(c0,4);
			qword c2L = si_rotqbyi(c2,4);
			qword c4L = si_rotqbyi(c4,4);
			// shuffle centres
			qword s_C0a0 = SHUF4(C,0,a,0);
			qword s_D0b0 = SHUF4(D,0,b,0);
			qword c1C = si_shufb(c1, r1, s_C0a0);
			qword c2C = si_shufb(l2, c2, s_D0b0);
			qword c3C = si_shufb(c3, r3, s_C0a0);
			qword c4C = si_shufb(l4, c4, s_D0b0);
			// average
			qword c1s = avg4(c0L, c1, c2L, c1C);
//			c1s = si_xor(c1s, random(2));
			qword c2s = avg4(c1, c2L, c3, c2C);
//			c2s = si_xor(c2s, random(2));
			qword c3s = avg4(c2L, c3, c4L, c3C);
//			c3s = si_xor(c3s, random(2));
			qword c4s = avg4(c3, c4L, c5, c4C);
//			c4s = si_xor(c4s, random(2));
			// write back
			qword s_AaCc = SHUF4(A,a,C,c);
			qword s_AbCd = SHUF4(A,b,C,d);
			a[b][u]  [v] = si_shufb(c1, c1s, s_AaCc);
			a[b][u+1][v] = si_shufb(c2s, c2, s_AbCd);
			a[b][u+2][v] = si_shufb(c3, c3s, s_AaCc);
			a[b][u+3][v] = si_shufb(c4s, c4, s_AbCd);
		}
	}
}


// render diamond-square into tile
static void ds(int b, int i) {
	int r = 1 << (7 - i);
	sqouter(b, i, r);
	sqinner(b, r);
	diouter(b, i, r);
	diinner(b, r);
	if(r>8) {
		// recurse
		ds(b, i+1);
	} else {
		// special case last two
		sqouter(b, 5, 4);
		sqinner4(b);
		diall4(b);
		sqouter(b, 6, 2);
		sqinner2(b);
		diall2(b);
	}
}



int main() __attribute__((flatten));
int main() {
	// Allocate some scratch space for syscalls and overdraw
	uint32_t size = getpagesize();
	unsigned long long mmap_res = mmap_eaddr(0ULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if(mmap_res == MAP_FAILED_EADDR) {
		perror("Could not mmap space for syscall use.");
		exit(EXIT_FAILURE);
	}

	void* cp_vt_syscall_space = (void*)(uint32_t)mmap_res;
	void* cp_fb_syscall_space = (void*)(uint32_t)(mmap_res + 128);

	// a scratch target - a place to dump unneeded data in a DMA list
	uint overdraw = (uint32_t)(mmap_res + 256);

	// Set up the vt and fb
	cp_vt vt;
	cp_fb fb;

	cp_vt_open_graphics(&vt, cp_vt_syscall_space);
	cp_fb_open(&fb, cp_fb_syscall_space);

	unsigned int line_bytes = fb.stride<<2;

	// DMA list for regular tiles
	mfc_list_element_t list[2][S];
	// set up the DMA list constants
	for(unsigned int q = 0; q < S; ++q) {
		list[0][q].notify = 0;
		list[0][q].size = sizeof(a[0][0]);
		list[1][q].notify = 0;
		list[1][q].size = sizeof(a[0][0]);
	}

	// calc tile dimensions.  Last tile always needs special handling
	unsigned int whole_tile_w = (fb.stride - 1)/S;
	// total number of tiles vertically.  Ensure either neat fit or draw an extra row
	unsigned int whole_tile_h = (fb.h+127)/S;

	// DMA lists for rhs tiles
	mfc_list_element_t list_rhs[2][S*2];
	// amount to be drawn
	int rhs_draw = fb.stride - whole_tile_w * S;
	// amount to throw away
	int rhs_skip = T4 * 4 - rhs_draw;
	// init fields
	for(unsigned int q = 0; q < S*2; q+=2) {
		list_rhs[0][q].notify = 0;
		list_rhs[0][q].size = rhs_draw * 4;
		list_rhs[0][q+1].notify = 0;
		list_rhs[0][q+1].size = rhs_skip * 4;
		list_rhs[0][q+1].eal = overdraw;
		list_rhs[1][q].notify = 0;
		list_rhs[1][q].size = rhs_draw * 4;
		list_rhs[1][q+1].notify = 0;
		list_rhs[1][q+1].size = rhs_skip * 4;
		list_rhs[1][q+1].eal = overdraw;
	}


	// generate 'random' values
	genseed();

	int frame_ndx = 0;

	// run a set number of frames
	for(int fr = 0; fr > -1; ++fr) {
		// track frame time
		spu_write_decrementer(-1);
		// buffer selector
		int b = 0;
		for(unsigned int i = 0; i < whole_tile_h; ++i) {

			// the amount of the dma list that will be needed for each tile in the row
			// will be all lines, except for bottom row which may require fewer
			unsigned int dma_list_size = (fb.h - i * S < S ? fb.h - i * S : S) * sizeof(list[b][0]);

			for(unsigned int j = 0; j < whole_tile_w+1; ++j) {

				// apply seed values to tile
				applyseed(b,i,j);
				// perform diamond-square interpolation
				ds(b,0);

				// sync previous iteration
				mfc_write_tag_mask(1<<b); spu_mfcstat(MFC_TAG_UPDATE_ALL);

				// jump out of the loop on the last iter, otherwise performance is terrible
				if(j == whole_tile_w) continue;

				// set up DMA list for this tile
				// (could be much improved, but overall takes negligible time)
				unsigned int base = fb.draw_addr[frame_ndx] + j*S*4 + (i*S)*line_bytes;
				for(unsigned int q = 0; q < S; ++q) {
					list[b][q].eal = base + q*line_bytes;
				}

				// Write out the tile.
				// Not aligned in local store. Writes extra trash to fb.  No problem as is overwritten
				// by next tile.
				//
				// Rightmost tile is handled specially outside of the loop.
				spu_mfcdma32(&a[b][P][P>>2], (uint32_t)&list[b][0], dma_list_size, b, MFC_PUTL_CMD);

				// flip buffer for next iter.
				b ^= 1;
			}

			// rhs tile
			// if these are performed here rather than in the loop, total frame time increases by 3ms o_0
//			applyseed(b, i, whole_tile_w);
//			ds(b,0);
//			mfc_write_tag_mask(1<<b); spu_mfcstat(MFC_TAG_UPDATE_ALL);

			// set up dma list to throw away part of every line
			unsigned int base = fb.draw_addr[frame_ndx] + whole_tile_w*S*4 + (i*S)*line_bytes;
			for(unsigned int q = 0; q < S; ++q) {
				list_rhs[b][q+q].eal = base + q*line_bytes;
			}

			// write out the special list
			spu_mfcdma32(&a[b][P][P>>2], (uint32_t)&list_rhs[b][0], 2*dma_list_size, b, MFC_PUTL_CMD);

			b ^= 1;

			tick();
		}

		// wait for end of frame and flip
		cp_fb_wait_vsync(&fb);
		cp_fb_flip(&fb, frame_ndx);
		frame_ndx ^= 1;

		// modify the random numbers for the next iter
		perturbseed();

		// print time for frame only if we failed to achieve 1/60th
		if(-spu_read_decrementer()/79800000. > 0.017f) {
			printf("%f\n", -spu_read_decrementer()/79800000.);
			printf("%d\n", fr);
		}
		tick_print_all();
		tick_reset();
	}

	cp_vt_close(&vt);
	cp_fb_close(&fb);

	return 0;
}

