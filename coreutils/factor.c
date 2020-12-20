/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FACTOR
//config:	bool "factor (2.7 kb)"
//config:	default y
//config:	help
//config:	factor factorizes integers

//applet:IF_FACTOR(APPLET(factor, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_FACTOR) += factor.o

//usage:#define factor_trivial_usage
//usage:       "[NUMBER]..."
//usage:#define factor_full_usage "\n\n"
//usage:       "Print prime factors"

#include "libbb.h"
#include "common_bufsiz.h"

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

typedef unsigned long long wide_t;

#if ULLONG_MAX == (UINT_MAX * UINT_MAX + 2 * UINT_MAX)
/* "unsigned" is half as wide as ullong */
typedef unsigned half_t;
#define HALF_MAX UINT_MAX
#define HALF_FMT ""
#elif ULLONG_MAX == (ULONG_MAX * ULONG_MAX + 2 * ULONG_MAX)
/* long is half as wide as ullong */
typedef unsigned long half_t;
#define HALF_MAX ULONG_MAX
#define HALF_FMT "l"
#else
#error Cant find an integer type which is half as wide as ullong
#endif

/* The trial divisor increment wheel.  Use it to skip over divisors that
 * are composites of 2, 3, 5, 7, or 11.
 * Larger wheels improve sieving only slightly, but quickly grow in size
 * (adding just one prime, 13, results in 5766 element sieve).
 */
#define R(a,b,c,d,e,f,g,h,i,j,A,B,C,D,E,F,G,H,I,J) \
	(((uint64_t)(a<<0) | (b<<3) | (c<<6) | (d<<9) | (e<<12) | (f<<15) | (g<<18) | (h<<21) | (i<<24) | (j<<27))      ) | \
	(((uint64_t)(A<<0) | (B<<3) | (C<<6) | (D<<9) | (E<<12) | (F<<15) | (G<<18) | (H<<21) | (I<<24) | (J<<27)) << 30) | \
	(((uint64_t)7 << 60))
#define P(a,b,c,d,e,f,g,h,i,j,A,B,C,D,E,F,G,H,I,J) \
	R(	(a/2-1),(b/2-1),(c/2-1),(d/2-1),(e/2-1),(f/2-1),(g/2-1),(h/2-1),(i/2-1),(j/2-1), \
		(A/2-1),(B/2-1),(C/2-1),(D/2-1),(E/2-1),(F/2-1),(G/2-1),(H/2-1),(I/2-1),(J/2-1)  )
static const uint64_t packed_wheel[] = {
	/*1, 2, 2, 4, 2,*/
	P( 4, 2, 4, 6, 2, 6, 4, 2, 4, 6, 6, 2, 6, 4, 2, 6, 4, 6, 8, 4), //01
	P( 2, 4, 2, 4,14, 4, 6, 2,10, 2, 6, 6, 4, 2, 4, 6, 2,10, 2, 4), //02
	P( 2,12,10, 2, 4, 2, 4, 6, 2, 6, 4, 6, 6, 6, 2, 6, 4, 2, 6, 4), //03
	P( 6, 8, 4, 2, 4, 6, 8, 6,10, 2, 4, 6, 2, 6, 6, 4, 2, 4, 6, 2), //04
	P( 6, 4, 2, 6,10, 2,10, 2, 4, 2, 4, 6, 8, 4, 2, 4,12, 2, 6, 4), //05
	P( 2, 6, 4, 6,12, 2, 4, 2, 4, 8, 6, 4, 6, 2, 4, 6, 2, 6,10, 2), //06
	P( 4, 6, 2, 6, 4, 2, 4, 2,10, 2,10, 2, 4, 6, 6, 2, 6, 6, 4, 6), //07
	P( 6, 2, 6, 4, 2, 6, 4, 6, 8, 4, 2, 6, 4, 8, 6, 4, 6, 2, 4, 6), //08
	P( 8, 6, 4, 2,10, 2, 6, 4, 2, 4, 2,10, 2,10, 2, 4, 2, 4, 8, 6), //09
	P( 4, 2, 4, 6, 6, 2, 6, 4, 8, 4, 6, 8, 4, 2, 4, 2, 4, 8, 6, 4), //10
	P( 6, 6, 6, 2, 6, 6, 4, 2, 4, 6, 2, 6, 4, 2, 4, 2,10, 2,10, 2), //11
	P( 6, 4, 6, 2, 6, 4, 2, 4, 6, 6, 8, 4, 2, 6,10, 8, 4, 2, 4, 2), //12
	P( 4, 8,10, 6, 2, 4, 8, 6, 6, 4, 2, 4, 6, 2, 6, 4, 6, 2,10, 2), //13
	P(10, 2, 4, 2, 4, 6, 2, 6, 4, 2, 4, 6, 6, 2, 6, 6, 6, 4, 6, 8), //14
	P( 4, 2, 4, 2, 4, 8, 6, 4, 8, 4, 6, 2, 6, 6, 4, 2, 4, 6, 8, 4), //15
	P( 2, 4, 2,10, 2,10, 2, 4, 2, 4, 6, 2,10, 2, 4, 6, 8, 6, 4, 2), //16
	P( 6, 4, 6, 8, 4, 6, 2, 4, 8, 6, 4, 6, 2, 4, 6, 2, 6, 6, 4, 6), //17
	P( 6, 2, 6, 6, 4, 2,10, 2,10, 2, 4, 2, 4, 6, 2, 6, 4, 2,10, 6), //18
	P( 2, 6, 4, 2, 6, 4, 6, 8, 4, 2, 4, 2,12, 6, 4, 6, 2, 4, 6, 2), //19
	P(12, 4, 2, 4, 8, 6, 4, 2, 4, 2,10, 2,10, 6, 2, 4, 6, 2, 6, 4), //20
	P( 2, 4, 6, 6, 2, 6, 4, 2,10, 6, 8, 6, 4, 2, 4, 8, 6, 4, 6, 2), //21
	P( 4, 6, 2, 6, 6, 6, 4, 6, 2, 6, 4, 2, 4, 2,10,12, 2, 4, 2,10), //22
	P( 2, 6, 4, 2, 4, 6, 6, 2,10, 2, 6, 4,14, 4, 2, 4, 2, 4, 8, 6), //23
	P( 4, 6, 2, 4, 6, 2, 6, 6, 4, 2, 4, 6, 2, 6, 4, 2, 4,12, 2,12), //24
};
#undef P
#undef r
#define WHEEL_START 5
#define WHEEL_SIZE (5 + 24 * 20)
#define wheel_tab ((uint8_t*)&bb_common_bufsiz1)
/*
 * Why, you ask?
 * plain byte array:
 *	function                old     new   delta
 *	wheel_tab                 -     485    +485
 * 3-bit-packed insanity:
 *	packed_wheel              -     192    +192
 *	factor_main             108     176     +68
 */
static void unpack_wheel(void)
{
	int i;
	uint8_t *p;

	setup_common_bufsiz();
	wheel_tab[0] = 1;
	wheel_tab[1] = 2;
	wheel_tab[2] = 2;
	wheel_tab[3] = 4;
	wheel_tab[4] = 2;
	p = &wheel_tab[5];
	for (i = 0; i < ARRAY_SIZE(packed_wheel); i++) {
		uint64_t v = packed_wheel[i];
		do {
			*p = ((unsigned)(v & 7) + 1) * 2;
			//printf("%2u,", *p);
			p++;
			v >>= 3;
		} while ((unsigned)v != 7);
		//printf("\n");
	}
}

static half_t isqrt_odd(wide_t N)
{
	half_t s = isqrt(N);
	/* Subtract 1 from even s, odd s won't change: */
	/* (doesnt work for zero, but we know that s != 0 here) */
	s = (s - 1) | 1;
	return s;
}

static NOINLINE void factorize(wide_t N)
{
	unsigned w;
	half_t factor;
	half_t max_factor;

	if (N < 4)
		goto end;

	/* The code needs to be optimized for the case where
	 * there are large prime factors. For example,
	 * this is not hard:
	 * 8262075252869367027 = 3 7 17 23 47 101 113 127 131 137 823
	 *  (the largest divisor to test for largest factor 823
	 *  is only ~sqrt(823) = 28, the entire factorization needs
	 *  only ~33 trial divisions)
	 * but this is:
	 * 18446744073709551601 = 53 348051774975651917
	 * the last factor requires testing up to
	 * 589959129 - about 100 million iterations.
	 * The slowest case (largest prime) for N < 2^64 is
	 * factor 18446744073709551557 (0xffffffffffffffc5).
	 */
	max_factor = isqrt_odd(N);
	factor = 2;
	w = 0;
	for (;;) {
		half_t fw;

		/* The division is the most costly part of the loop.
		 * On 64bit CPUs, takes at best 12 cycles, often ~20.
		 */
		while ((N % factor) == 0) { /* not likely */
			N = N / factor;
			printf(" %"HALF_FMT"u", factor);
			max_factor = isqrt_odd(N);
		}
		if (factor >= max_factor)
			break;
		fw = factor + wheel_tab[w];
		if (fw < factor)
			break; /* overflow */
		factor = fw;
		w++;
		if (w < WHEEL_SIZE)
			continue;
		w = WHEEL_START;
	}
 end:
	if (N > 1)
		printf(" %llu", N);
	bb_putchar('\n');
}

static void factorize_numstr(const char *numstr)
{
	wide_t N;

	/* Leading + is ok (coreutils compat) */
	if (*numstr == '+')
		numstr++;
	N = bb_strtoull(numstr, NULL, 10);
	if (errno)
		bb_show_usage();
	printf("%llu:", N);
	factorize(N);
}

int factor_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int factor_main(int argc UNUSED_PARAM, char **argv)
{
	unpack_wheel();

	//// coreutils has undocumented option ---debug (three dashes)
	//getopt32(argv, "");
	//argv += optind;
	argv++;

	if (!*argv) {
		/* Read from stdin, several numbers per line are accepted */
		for (;;) {
			char *numstr, *line;
			line = xmalloc_fgetline(stdin);
			if (!line)
				return EXIT_SUCCESS;
			numstr = line;
			for (;;) {
				char *end;
				numstr = skip_whitespace(numstr);
				if (!numstr[0])
					break;
				end = skip_non_whitespace(numstr);
				if (*end != '\0')
					*end++ = '\0';
				factorize_numstr(numstr);
				numstr = end;
			}
			free(line);
		}
	}

	do {
		/* Leading spaces are ok (coreutils compat) */
		factorize_numstr(skip_whitespace(*argv));
	} while (*++argv);

	return EXIT_SUCCESS;
}
