/*
   common.h: Include standard includes, define helpful macros, and
   define functions from common.c.

   Copyright (C) 2006 Daniel Stutzbach

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef COMMON_H
#define COMMON_H

#undef min
#undef max

/* Need this to get M_PI out of math.h */
#define _XOPEN_SOURCE 1
/* Needed for useful goodies like ffsl() */
#define _GNU_SOURCE 1
#define __USE_GNU 1 /* Needed for asprintf() */
/* Needed for still more useful goodies, like log2() */
#define __USE_ISOC99 1

/* Standard includes to save time on typing */
#include <stdint.h>
#include <sys/types.h>
#ifndef __MINGW32__
#include <regex.h>
#else
#define random rand
#define srandom srand
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <search.h>

#if __WORDSIZE == 32
typedef int pint;
#elif __WORDSIZE == 64
typedef long pint;
#else
#error Unknown pointer size
#endif

char *common_init (const char *s);
double get_num (char **s);
char *get_word (char **s);
char *eat_white (char *s);
char *get_eol (char **s);
char **get_lines (char **ps, int n);

char **gen_buffer (int x, int y);
void flush_buffer (char **lines, int y);
void free_buffer (char **lines, int y);

typedef int bool;
typedef int compare_t (const void *v1, const void *v2);

inline static void nothing(int x __attribute__ ((unused)), ...) {}

#define __unused __attribute__ ((unused))
#define _unused(x) x __attribute__ ((unused))

void _die (const char *func, unsigned line) __attribute__ ((noreturn));
#define die() _die(__FILE__, __LINE__)

#define lambda(r, p, args...) ({r _f p args _f;})
#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))
#define swap(x,y) do {typeof (x) t_; t_ = (x); (x) = (y); (y) = t_;} while(0)
#define cmp3(x,y) ((x) < (y) ? -1 : (x) > (y) ? 1 : 0)
#define sq(x) ((x) * (x))
#define sign(x) (is_zero (x) ? 0 : (x) < (typeof (x)) 0 ? -1 : 1)

#define myalloc(x) ((x) = calloc (sizeof (*(x)), 1))
#define myallocn(x,n) ((x) = calloc (sizeof (*(x)), n))
#define myrealloc(x,n) ((x) = realloc ((x), sizeof (*(x)) * (n)))
#define grow(x,m,f) ((m) > (f) ? 0 : ({\
        while ((m) <= (f)) myrealloc ((x), (m) <<= 1); 1;}))

/* Floating point stuff */
typedef double ftype_t;
typedef float cost_t;
static const ftype_t epsilon = 0.0000001;
#define ftoi(x) (nearbyint ((x)))
#define fcmp3(x,y) (feq ((x),(y)) ? 0 : (x) < (y) ? -1 : 1)
#define is_zero(x) ((x) < epsilon && (x) > -epsilon)
#define zeroify(x) (is_zero (x) ? (typeof (x)) 0.0 : (x))
#define feq(x,y) (fabs ((x) - (y)) < epsilon)
#define fne(x,y) (!feq ((x), (y)))
#define fge(x,y) ((x) >= (y) - epsilon)
#define fle(x,y) ((x) <= (y) + epsilon)

#define determinant2(a,b,c,d) ((a)*(d) - (b)*(c))
#define determinant3(a1,a2,a3,b1,b2,b3,c1,c2,c3) \
        ((a1)*(b2)*(c3) - (a1)*(b3)*(c2) \
         - (a2)*(b1)*(c3) + (a2)*(b3)*(c1) \
         + (a3)*(b1)*(c2) - (a3)*(b2)*(c1))

/* Things related to ranges (needed for geometry)*/
#define between(x,m1,m2) (((x) >= (m1) && (x) <= (m2)) \
                          || ((x) >= (m2) && (x) <= (m1)))
#define fbetween(x,m1,m2) ((fge ((x), (m1)) && fle ((x), (m2))) \
                           || (fge ((x), (m2)) && fle ((x), (m1))))
enum overlap overlap (ftype_t r1a, ftype_t r1b, ftype_t r2a, ftype_t r2b);
enum overlap {
        OVERLAP_NONE = 0,
        OVERLAP_POINT = 1,
        OVERLAP_MANY = 2
};

/* Less frequently used stuff */
static inline long mod (long x, long m) {
        while (x < 0) x += m;
        return x % m;
}

//! Remove duplicates from a sorted array.  Returns number of remaining items
size_t remove_dups (void *base, size_t nmemb, size_t size,
                    int (*compar) (const void *, const void *));
#define array_len(x) (sizeof (x) / sizeof ((x)[0]))
static inline void *_anydup(const void *x, size_t s) {
        void *v = malloc (s);
        memcpy (v, x, s);
        return v;
}
#define anydup(x) (_anydup ((x), sizeof (*(x))))

#define string_cmp ((compare_t *) strcmp)
int fcompare (const void *v1, const void *v2); //!< ftype_t *
int icompare (const void *v1, const void *v2); //!< int *
int pcompare (const void *pv1, const void *pv2); //!< void **
int vcompare (const void *v1, const void *v2); //!< void *

/* Returns start through last-1 */
int randrange (int start, int last);
double real_random(void);

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
//#include <obstack.h>

#ifdef DEBUG
#define dprintf(format, args...) printf (format , ## args)
#define trace() (printf ("%s:%d\n", __FILE__, __LINE__), fflush(stdout))
#else
#define dprintf(format, args...) (nothing (0, format , ## args))
#define trace() (nothing (0))
#endif

#define False 0
#define True 1

#endif
