#ifndef SHUFFLE_GENERATOR_H_
#define SHUFFLE_GENERATOR_H_

/*
 * Generate shuffle patterns with minimal fuss.
 * Based on a naming mechanism published on insomniac r&d page
 * http://www.insomniacgames.com/tech/articles/0408/shufflehelper.php
 *
 * Written by Jonathan Adamczewski, 2010.  <jonathan@brnz.org>
 *
 * A-P indicates 0-15th position in first vector
 * a-p indicates 0-15th position in second vector
 * x or X indicates 0xff
 * 8 indicates 0x80
 * 0 indicates 0x00
 *
 * The macros SHUF4() SHUF8() and SHUF16() provides a vector literal
 * pattern consistent with the letters provided named shuf_pattern.
 * Good for using in si_shufb() calls.
 * TODO: wrap si_shufb() in a way that patter spec can be included
 * 		 directly in the call without the extra macro invocation.
 *
 *
 * For example :
 * SHUF4(A,A,A,A)
 * is equivalent to :
 * ((qword){0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3})
 *
 * DECL_SHUF{4,8,16}()declare a const qword in the same way
 * DECL_SHUF4(A,A,A,A);
 * is equivalent to :
 * const qword shuf_AAAA = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
 *
 * Could do with some reengineering - horribly namespace polluting,
 * unclear in function and probably dangerous to use in all sorts
 * of ways.  Could probably be compacted with better use of
 * preprocessor functionality.
 *
 */

#include <spu_intrinsics.h>

#define SHUF4A  0x00, 0x01, 0x02, 0x03
#define SHUF4B  0x04, 0x05, 0x06, 0x07
#define SHUF4C  0x08, 0x09, 0x0a, 0x0b
#define SHUF4D  0x0c, 0x0d, 0x0e, 0x0f
#define SHUF4a  0x10, 0x11, 0x12, 0x13
#define SHUF4b  0x14, 0x15, 0x16, 0x17
#define SHUF4c  0x18, 0x19, 0x1a, 0x1b
#define SHUF4d  0x1c, 0x1d, 0x1e, 0x1f
#define SHUF4X  0xc0, 0xc0, 0xc0, 0xc0
#define SHUF4x  0xc0, 0xc0, 0xc0, 0xc0
#define SHUF40  0x80, 0x80, 0x80, 0x80
#define SHUF48  0xe0, 0xe0, 0xe0, 0xe0

#define DECL_SHUF4(A, B, C, D) \
		const qword shuf_##A##B##C##D = { \
			SHUF4##A, \
			SHUF4##B, \
			SHUF4##C, \
			SHUF4##D \
		}

#define SHUF4(A, B, C, D) \
		((qword){ \
			SHUF4##A, \
			SHUF4##B, \
			SHUF4##C, \
			SHUF4##D \
		})

#define SHUF8A  0x00, 0x01
#define SHUF8B  0x02, 0x03
#define SHUF8C  0x04, 0x05
#define SHUF8D  0x06, 0x07
#define SHUF8E  0x08, 0x09
#define SHUF8F  0x0a, 0x0b
#define SHUF8G  0x0c, 0x0d
#define SHUF8H  0x0e, 0x0f
#define SHUF8a  0x10, 0x11
#define SHUF8b  0x12, 0x13
#define SHUF8c  0x14, 0x15
#define SHUF8d  0x16, 0x17
#define SHUF8e  0x18, 0x19
#define SHUF8f  0x1a, 0x1b
#define SHUF8g  0x1c, 0x1d
#define SHUF8h  0x1e, 0x1f
#define SHUF8X  0xc0, 0xc0
#define SHUF8x  0xc0, 0xc0
#define SHUF80  0x80, 0x80
#define SHUF88  0xe0, 0xe0

#define DECL_SHUF8(A, B, C, D, E, F, G, H) \
		const qword shuf_##A##B##C##D##E##F##G##H = { \
				SHUF8##A, \
				SHUF8##B, \
				SHUF8##C, \
				SHUF8##D, \
				SHUF8##E, \
				SHUF8##F, \
				SHUF8##G, \
				SHUF8##H \
		}

#define SHUF8(A, B, C, D, E, F, G, H) \
		((qword){ \
				SHUF8##A, \
				SHUF8##B, \
				SHUF8##C, \
				SHUF8##D, \
				SHUF8##E, \
				SHUF8##F, \
				SHUF8##G, \
				SHUF8##H \
		})

#define SHUF16A  0x00
#define SHUF16B  0x01
#define SHUF16C  0x02
#define SHUF16D  0x03
#define SHUF16E  0x04
#define SHUF16F  0x05
#define SHUF16G  0x06
#define SHUF16H  0x07
#define SHUF16I  0x08
#define SHUF16J  0x09
#define SHUF16K  0x0a
#define SHUF16L  0x0b
#define SHUF16M  0x0c
#define SHUF16N  0x0d
#define SHUF16O  0x0e
#define SHUF16P  0x0f
#define SHUF16a  0x10
#define SHUF16b  0x11
#define SHUF16c  0x12
#define SHUF16d  0x13
#define SHUF16e  0x14
#define SHUF16f  0x15
#define SHUF16g  0x16
#define SHUF16h  0x17
#define SHUF16i  0x18
#define SHUF16j  0x19
#define SHUF16k  0x1a
#define SHUF16l  0x1b
#define SHUF16m  0x1c
#define SHUF16n  0x1d
#define SHUF16o  0x1e
#define SHUF16p  0x1f
#define SHUF16X  0xc0
#define SHUF16x  0xc0
#define SHUF160  0x80
#define SHUF168  0xe0

#define DECL_SHUF16(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P) \
		const qword shuf_##A##B##C##D##E##F##G##H##I##J##K##L##M##N##O##P = { \
				SHUF16##A, \
				SHUF16##B, \
				SHUF16##C, \
				SHUF16##D, \
				SHUF16##E, \
				SHUF16##F, \
				SHUF16##G, \
				SHUF16##H, \
				SHUF16##I, \
				SHUF16##J, \
				SHUF16##K, \
				SHUF16##L, \
				SHUF16##M, \
				SHUF16##N, \
				SHUF16##O, \
				SHUF16##P \
		}

#define SHUF16(A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P) \
		((qword) { \
				SHUF16##A, \
				SHUF16##B, \
				SHUF16##C, \
				SHUF16##D, \
				SHUF16##E, \
				SHUF16##F, \
				SHUF16##G, \
				SHUF16##H, \
				SHUF16##I, \
				SHUF16##J, \
				SHUF16##K, \
				SHUF16##L, \
				SHUF16##M, \
				SHUF16##N, \
				SHUF16##O, \
				SHUF16##P \
		})

#endif


#if 0
// For further consideration...
// (Is not quite correct, but will do the right kind of thing...)
namespace shufflor {
	struct A{enum{val=0};};
	struct B{enum{val=1};};
	struct C{enum{val=2};};
	struct D{enum{val=3};};

	template<typename a, typename b, typename c, typename d>
	qword SHUF() {
		return (qword){a::val,b::val,c::val,d::val};
	}

	qword tst = SHUF<A,B,C,D>();
}
#endif
