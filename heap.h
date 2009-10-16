/*
   heap.h: A min-heap implementation, header for heap.c.

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

#ifndef __HEAP_H
#define __HEAP_H

#include "common.h"

struct heap;
typedef int heap_loc_t;

/*! Used to create a new heap.  compare() is an arbitrary compare
 *  function.  It must be consistent and create a unique ordering of
 *  the data.  If you invert the sense of the compare function, you'll
 *  get a max-heap instead of a min-heap.
 *
 *  The "offset" argument requires some explination.  It is required
 *  if the heap_decrease_key(), heap_in(), or heap_remove() functions
 *  will be used.  Otherwise, pass in -1 to disable this feature.  It
 *  represents the byte offset of a heap_loc_t field inside of the
 *  opaque data structure being used by the caller.  This is normally
 *  computed using the offsetof() C macro.
 *
 *  For example, here is a min-heap of integers:
 *          struct foo {
 *                  int x;
 *                  heap_loc_t heap_loc;
 *          };
 *
 *          struct heap *heap
 *                  = heap_new (icompare, offsetof (struct foo, heap_loc));
 */
struct heap *heap_new (int (*compare) (const void *v1, const void *v2),
                       int offset);

unsigned heap_len(struct heap *heap);
bool heap_empty (struct heap *heap);
void *heap_peek (struct heap *heap);
void *heap_extract_min (struct heap *heap);
void heap_insert (struct heap *heap, void *v);
void heap_delete (struct heap *heap);

//! Parameters qsort(), but with worst-case run time of O(n*lg(n))
void heapsort (void *base, size_t nmemb, size_t size, compare_t *compare);

/* Functions below here require that the "offset" feature is used */

void heap_decrease_key (struct heap *heap, void *v);
void heap_remove(struct heap *heap, void *v);

//! This function is only meaningful if v was at one time in the heap
bool heap_in (struct heap *heap, void *v);


#endif
