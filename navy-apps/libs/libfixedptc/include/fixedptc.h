#ifndef _FIXEDPTC_H_
#define _FIXEDPTC_H_

#ifndef FIXEDPT_BITS
#define FIXEDPT_BITS	32
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if FIXEDPT_BITS == 32
typedef int32_t fixedpt;
typedef	int64_t	fixedptd;
typedef	uint32_t fixedptu;
typedef	uint64_t fixedptud;
#elif FIXEDPT_BITS == 64
typedef int64_t fixedpt;
typedef	__int128_t fixedptd;
typedef	uint64_t fixedptu;
typedef	__uint128_t fixedptud;
#else
#error "FIXEDPT_BITS must be equal to 32 or 64"
#endif

#ifndef FIXEDPT_WBITS
#define FIXEDPT_WBITS	24
#endif

#if FIXEDPT_WBITS >= FIXEDPT_BITS
#error "FIXEDPT_WBITS must be less than or equal to FIXEDPT_BITS"
#endif

#define FIXEDPT_VCSID "$Id$"

#define FIXEDPT_FBITS	(FIXEDPT_BITS - FIXEDPT_WBITS)
#define FIXEDPT_FMASK	(((fixedpt)1 << FIXEDPT_FBITS) - 1)

#define fixedpt_rconst(R) ((fixedpt)((R) * FIXEDPT_ONE + ((R) >= 0 ? 0.5 : -0.5)))
#define fixedpt_fromint(I) ((fixedptd)(I) << FIXEDPT_FBITS)
#define fixedpt_toint(F) ((F) >> FIXEDPT_FBITS)
#define fixedpt_add(A,B) ((A) + (B))
#define fixedpt_sub(A,B) ((A) - (B))
#define fixedpt_fracpart(A) ((fixedpt)(A) & FIXEDPT_FMASK)

#define FIXEDPT_ONE	((fixedpt)((fixedpt)1 << FIXEDPT_FBITS))
#define FIXEDPT_ONE_HALF (FIXEDPT_ONE >> 1)
#define FIXEDPT_TWO	(FIXEDPT_ONE + FIXEDPT_ONE)
#define FIXEDPT_PI	fixedpt_rconst(3.14159265358979323846)
#define FIXEDPT_TWO_PI	fixedpt_rconst(2 * 3.14159265358979323846)
#define FIXEDPT_HALF_PI	fixedpt_rconst(3.14159265358979323846 / 2)
#define FIXEDPT_E	fixedpt_rconst(2.7182818284590452354)

/* fixedpt is meant to be usable in environments without floating point support
 * (e.g. microcontrollers, kernels), so we can't use floating point types directly.
 * Putting them only in macros will effectively make them optional. */
#define fixedpt_tofloat(T) ((float) ((T)*((float)(1)/(float)(1L << FIXEDPT_FBITS))))

/* Multiplies a fixedpt number with an integer, returns the result. */
static inline fixedpt fixedpt_muli(fixedpt A, int B) {
	return A * (fixedpt)B;
}

/* Divides a fixedpt number with an integer, returns the result. */
static inline fixedpt fixedpt_divi(fixedpt A, int B) {
	return A / (fixedpt)B;
}

/* Multiplies two fixedpt numbers, returns the result. */
static inline fixedpt fixedpt_mul(fixedpt A, fixedpt B) {
	return (fixedpt)(((fixedptd)A * (fixedptd)B) >> FIXEDPT_FBITS);
}

/* Divides two fixedpt numbers, returns the result. */
static inline fixedpt fixedpt_div(fixedpt A, fixedpt B) {
	return (fixedpt)(((fixedptd)A << FIXEDPT_FBITS) / (fixedptd)B);
}

static inline fixedpt fixedpt_abs(fixedpt A) {
	return (A < 0) ? -A : A;
}

static inline fixedpt fixedpt_floor(fixedpt A) {
	fixedpt frac = fixedpt_fracpart(A);
	if (frac == 0) return A;
	if (A >= 0) {
		return A & ~FIXEDPT_FMASK;
	} else {
		return (A & ~FIXEDPT_FMASK) - FIXEDPT_ONE;
	}
}

static inline fixedpt fixedpt_ceil(fixedpt A) {
	fixedpt frac = fixedpt_fracpart(A);
	if (frac == 0) return A;
	if (A >= 0) {
		return (A & ~FIXEDPT_FMASK) + FIXEDPT_ONE;
	} else {
		return A & ~FIXEDPT_FMASK;
	}
}


void fixedpt_str(fixedpt A, char *str, int max_dec);

/* Converts the given fixedpt number into a string, using a static
 * (non-threadsafe) string buffer */
static inline char* fixedpt_cstr(const fixedpt A, const int max_dec) {
	static char str[25];

	fixedpt_str(A, str, max_dec);
	return (str);
}


/* Returns the square root of the given number, or -1 in case of error */
fixedpt fixedpt_sqrt(fixedpt A);


/* Returns the sine of the given fixedpt number. 
 * Note: the loss of precision is extraordinary! */
fixedpt fixedpt_sin(fixedpt fp);


/* Returns the cosine of the given fixedpt number */
static inline fixedpt fixedpt_cos(fixedpt A) {
	return (fixedpt_sin(FIXEDPT_HALF_PI - A));
}


/* Returns the tangens of the given fixedpt number */
static inline fixedpt fixedpt_tan(fixedpt A) {
	return fixedpt_div(fixedpt_sin(A), fixedpt_cos(A));
}


/* Returns the value exp(x), i.e. e^x of the given fixedpt number. */
fixedpt fixedpt_exp(fixedpt fp);


/* Returns the natural logarithm of the given fixedpt number. */
fixedpt fixedpt_ln(fixedpt x);


/* Returns the logarithm of the given base of the given fixedpt number */
static inline fixedpt fixedpt_log(fixedpt x, fixedpt base) {
	return (fixedpt_div(fixedpt_ln(x), fixedpt_ln(base)));
}


/* Return the power value (n^exp) of the given fixedpt numbers */
static inline fixedpt fixedpt_pow(fixedpt n, fixedpt exp) {
	if (exp == 0)
		return (FIXEDPT_ONE);
	if (n < 0)
		return 0;
	return (fixedpt_exp(fixedpt_mul(fixedpt_ln(n), exp)));
}

#ifdef __cplusplus
}
#endif

#endif
