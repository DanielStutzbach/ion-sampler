/*
   common.c: Define some commonly useful functions.

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

#include <time.h>
//#include <mcheck.h>
#include <signal.h>
#include "common.h"

void _die (const char *func, unsigned line)
{
        printf ("%s:%d: Died!\n", func, line);
        fflush(stdout);
        // raise (SIGTRAP);
        abort ();
}

/* A larger value is more efficient, but a smaller value means the
 * code actually gets tested on a regular basis */
#define BUF_SIZE (1)

char *common_init (const char *s)
{
        FILE *f;
        size_t max = BUF_SIZE;
        size_t len = 0;
        size_t now;
        char *buffer = malloc (max + 1);

        if (!s) f = stdin;
        else if (!(f = fopen (s, "r"))) die ();

        now = fread (buffer, 1, max - len, f);
        while (now == max - len) {
                max <<= 1;
                buffer = realloc (buffer, max + 1);
                len += now;
                now = fread (&buffer[len], 1, max - len, f);
        }

        len += now;
        buffer[len] = 0;        

        srandom (time (NULL));

        return buffer;
}

double get_num (char **s)
{
        return strtod (*s, s);
}

char *get_word (char **s)
{
        char *rv;
        *s = eat_white (*s);
        rv = *s;
        while (**s && !isspace (**s)) (*s)++;
        if (**s) { **s = 0; (*s)++; }
        return rv;        
}

char *eat_white (char *s)
{
        while (isspace (*s)) s++;
        return s;
}

char *get_eol (char **ps)
{
        char *here;
        char *s = *ps;
        here = s = eat_white (s);
        while (*s && *s != '\n') s++;
        if (*s) *s++ = 0;
        *ps = s;
        return here;        
}

/* Treating I/O as a big buffer */

char **get_lines (char **ps, int n)
{
        char **lines = malloc (sizeof (char *) * n);
        int i;
        for (i = 0; i < n; i++)
                lines[i] = get_eol (ps);
        return lines;
}

char **gen_buffer (int x, int y)
{
        char **lines = malloc (sizeof (char *) * y);
        int i;
        for (i = 0; i < y; i++) {
                lines[i] = malloc (sizeof (char) * (x + 1));
                memset (lines[i], ' ', sizeof (char) * x);
                lines[i][x] = 0;
        }
        return lines;
}

void flush_buffer (char **lines, int y)
{
        int i;
        for (i = 0; i < y; i++)
                printf ("%s\n", lines[i]);
}

void free_buffer (char **lines, int y)
{
        int i;
        for (i = 0; i < y; i++)
                free (lines[i]);
        free (lines);
}

/* Less frequently used things */

enum overlap overlap (ftype_t r1a, ftype_t r1b, ftype_t r2a, ftype_t r2b)
{
        if (r1a > r1b) swap (r1a, r1b);
        if (r2a > r2b) swap (r2a, r2b);

        if (r2a < r1a) {
                swap (r1a, r2a);
                swap (r1b, r2b);
        }

        if (feq (r1b, r2a)) return OVERLAP_POINT;
        if (r1b < r2a) return OVERLAP_NONE;
        return OVERLAP_MANY;
}

size_t remove_dups (void *base, size_t nmemb, size_t size,
                    int (*compar) (const void *, const void *))
{
        size_t d, s;
        char *b = base;

        if (nmemb < 2) return nmemb;

        for (d = 1, s = 0; s < nmemb - 1; s++) {
                if (compar (&b[size * s], &b[size * (s + 1)])) {
                        memmove (&b[size * d], &b[size * (s + 1)],size);
                        d++;
                }
        }

        return d;
}

//! ftype_t *
int fcompare (const void *v1, const void *v2)
{
        const ftype_t *f1 = v1;
        const ftype_t *f2 = v2;

        return fcmp3 (*f1, *f2);
}

//! int *
int icompare (const void *v1, const void *v2)
{
        const int *i1 = v1;
        const int *i2 = v2;

        return cmp3 (*i1, *i2);
}

//! void **
int pcompare (const void *pv1, const void *pv2)
{
        const void **v1 = (const void **) pv1;
        const void **v2 = (const void **) pv2;
        return cmp3 (*v1, *v2);
}

//! void *
int vcompare (const void *v1, const void *v2)
{
        return cmp3 (v1, v2);
}

/* Returns start through last-1 */
int randrange (int start, int last)
{
        unsigned range;
        if (start > last) swap (start, last);
        range = last - start;

        return random () % range + start;
}

double real_random(void)
{
        return random() / (double) RAND_MAX;
}

