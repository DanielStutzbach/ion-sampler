/*
   heap.c: A min-heap implementation.

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

#include "common.h"
#include "heap.h"

/* \addindex Heap
 * \addindex Priority Queue
 * \addindex Sorting!Heapsort
 */

struct heap
{
        int (*compare) (const void *v1, const void *v2);
        int offset;  //!< offset into void * to find a heap_loc_t
        void **root;
        int max_n;
        int n;
};

struct heap *heap_new (int (*compare) (const void *v1, const void *v2),
                       int offset)
{
        struct heap *heap = malloc (sizeof (struct heap));
        heap->compare = compare;
        heap->max_n = 32;
        heap->n = 0;
        heap->root = malloc (sizeof (void *) * heap->max_n);
        heap->offset = offset;
        return heap;
}

#define off(i) (*(heap_loc_t *) &(((char *)heap->root[(i)])[heap->offset]))
#define off2(v) (*(heap_loc_t *) &(((char *)(v))[heap->offset]))
static void heap_swap (struct heap *heap, unsigned i, unsigned j)
{
        swap (heap->root[i], heap->root[j]);
        if (heap->offset >= 0) swap (off (i), off (j));        
}

static void set (struct heap *heap, unsigned i, void *v)
{
        heap->root[i] = v;
        if (heap->offset >= 0) off (i) = i;
}

static void clear (struct heap *heap, void *v)
{
        if (heap->offset >= 0) off2 (v) = -1;
}

bool heap_in (struct heap *heap, void *v)
{
        if (heap->offset < 0) die ();
        return off2 (v) >= 0;
}

bool heap_empty (struct heap *heap)
{
        return !heap->n;
}

void *heap_peek (struct heap *heap)
{
        if (!heap->root) die ();
        return heap->root[0];        
}

#define PARENT(i) ((i - 1) / 2)
#define LEFT(i)   (2 * i + 1)
#define RIGHT(i)  (2 * (i + 1))

static void min_heapify (struct heap *heap, heap_loc_t i)
{
        heap_loc_t l, r;
        heap_loc_t min;
        l = LEFT (i);
        r = RIGHT (i);
        if (l < heap->n && (heap->compare (heap->root[l], heap->root[i]) < 0))
                min = l;
        else min = i;
        if (r < heap->n && (heap->compare (heap->root[r], heap->root[min]) <0))
                min = r;
        if (min != i) {
                heap_swap (heap, i, min);
                min_heapify (heap, min);
        }
}

void *heap_extract_min (struct heap *heap)
{
        void *data;
        if (!heap->root) die ();
        if (!heap->n) die ();
        data = heap->root[0];
        clear (heap, data);
        if (--heap->n) {
                set (heap, 0, heap->root[heap->n]);
                min_heapify (heap, 0);
        }

        return data;
}

static void decrease_key (struct heap *heap, heap_loc_t i)
{
        while (i > 0
               && heap->compare (heap->root[PARENT (i)], heap->root[i]) > 0) {
                heap_swap (heap, i, PARENT (i));
                i = PARENT (i);
        }
}

void heap_remove(struct heap *heap, void *v)
{
        heap_loc_t i;

        if (!heap_in(heap, v)) die();

        i = off2(v);
        while (i > 0) {
                heap_swap(heap, i, PARENT(i));
                i = PARENT(i);
        }

        heap_extract_min(heap);
}

void heap_insert (struct heap *heap, void *v)
{
        if (++heap->n > heap->max_n) {
                if (!heap->max_n) heap->max_n++;
                heap->root = realloc (heap->root, (heap->max_n <<= 1) *
                                      sizeof (void *));
        }
        set (heap, heap->n - 1, v);
        decrease_key (heap, heap->n - 1);
}

void heap_delete (struct heap *heap)
{
        if (heap->root) free (heap->root);
        free (heap);
}

void heap_decrease_key (struct heap *heap, void *v)
{
        heap_loc_t loc = off2 (v);
        if (heap->offset < 0) die ();
        decrease_key (heap, loc);
}

/*! Same arguments as qsort().  Uses (much) more memory, but has a
 *  better worst-case run time.  Occasionally useful if you're
 *  expecting pathological input data (e.g., a large number of
 *  identical elements).
 */
void heapsort (void *base, size_t nmemb, size_t size, compare_t *compare)
{
        uint8_t *sorted = malloc (nmemb * size);
        uint8_t *unsorted = base;
        struct heap *heap;
        unsigned i;
        
        heap = heap_new (compare, -1);
        for (i = 0; i < nmemb; i++)
                heap_insert (heap, &unsorted[size*i]);
        for (i = 0; i < nmemb; i++)
                memcpy (&sorted[size*i], heap_extract_min (heap), size);
        memcpy (base, sorted, size * nmemb);
        heap_delete (heap);
}

unsigned heap_len(struct heap *heap)
{
        return heap->n;
}

/* Test code is below here */

#if 0
static int compare (const void *v1, const void *v2)
{
        int i1 = (int) v1;
        int i2 = (int) v2;
        return i1 - i2;
}

int main (void) __attribute__ ((weak));
int main (void)
{
        struct heap *heap = heap_new (compare, -1);
        int x;
        
        heap_insert (heap, (void *) 5);
        heap_insert (heap, (void *) 7);
        heap_insert (heap, (void *) 3);
        heap_insert (heap, (void *) 120);
        heap_insert (heap, (void *) 1);
        heap_insert (heap, (void *) 3);
        heap_insert (heap, (void *) 95);

        heap_extract_min (heap);
        heap_extract_min (heap);
        heap_extract_min (heap);
        heap_extract_min (heap);
        heap_insert (heap, (void *) 1);
        heap_insert (heap, (void *) 5);
        heap_insert (heap, (void *) 3);
        heap_insert (heap, (void *) 3);

        heap_insert (heap, (void *) 8);

        while (!heap_empty (heap)) {
                x = (int) heap_extract_min (heap);
                printf ("%d ", x);
        }
        printf ("\n");

        heap_insert (heap, (void *) 1);
        heap_extract_min (heap);
        heap_insert (heap, (void *) 1);
        heap_extract_min (heap);
        heap_insert (heap, (void *) 1);
        heap_extract_min (heap);
        heap_insert (heap, (void *) 1);
        heap_extract_min (heap);
        heap_insert (heap, (void *) 1);
        heap_extract_min (heap);

        heap_delete (heap);

        return 0;
}
#endif
