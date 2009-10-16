/*
   queue.c: An efficient double-ended queue implementation.

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

#include "queue.h"

/* \addindex Queue
 * \addindex Stack
 * \addindex FIFO
 * \addindex LIFO
 */

struct queue
{
        void **data;
        size_t max_elems;
        size_t start;
        size_t n;
};

struct queue *queue_new (void)
{
        struct queue *q = malloc (sizeof (struct queue));
        q->max_elems = 1;
        q->data = malloc (sizeof (void *) * q->max_elems);
        q->start = 0;
        q->n = 0;
        return q;
}

void queue_push (struct queue *q, void *data)
{
        if (q->n == q->max_elems) {
                size_t new_max = q->max_elems << 1;
                q->data = realloc (q->data, sizeof (void *) * new_max);
                memmove (&q->data[q->start + q->max_elems],
                         &q->data[q->start],
                         sizeof (void *) * (q->max_elems - q->start));
                q->start += q->max_elems;
                q->max_elems = new_max;
        }

        q->data[(q->start + q->n++) % q->max_elems] = data;
}

void *queue_pop (struct queue *q)
{
        void *rv;
        if (!q->n--) die ();

        rv = q->data[q->start++];
        q->start %= q->max_elems;
        return rv;
}

void *stack_pop (struct stack *q)
{
        void *rv;
        if (!q->n--) die ();

        rv = q->data[(q->start + q->n) % q->max_elems];
        return rv;
}

bool queue_empty (struct queue *q) { return !q->n; }

int queue_len (struct queue *q) { return q->n; }

void *queue_peek (struct queue *q, int i)
{
        if (!q->n) die ();
        return q->data[(q->start + i) % q->max_elems];
}

void *stack_peek (struct stack *q, int i)
{
        if (!q->n) die();
        return q->data[(q->start + q->n - 1 - i) % q->max_elems];
}

void queue_delete (struct queue *q)
{
        free (q->data);
        free (q);
}

#if 0
/* Test code */

int main (void) __attribute__ ((weak));
int main (void)
{
        struct queue *q = queue_new ();

        queue_push (q, (void *) 1);
        queue_push (q, (void *) 2);
        queue_push (q, (void *) 3);
        queue_push (q, (void *) 4);
        queue_push (q, (void *) 5);
        queue_push (q, (void *) 6);

        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        queue_push (q, (void *) 7);
        queue_push (q, (void *) 8);
        queue_push (q, (void *) 9);
        queue_push (q, (void *) 10);
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));
        printf ("%d\n", (int) queue_pop (q));

        stack_push (q, (void *) 1);
        stack_push (q, (void *) 2);
        stack_push (q, (void *) 3);
        stack_push (q, (void *) 4);
        stack_push (q, (void *) 5);
        stack_push (q, (void *) 6);

        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        stack_push (q, (void *) 7);
        stack_push (q, (void *) 8);
        stack_push (q, (void *) 9);
        stack_push (q, (void *) 10);
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));
        printf ("%d\n", (int) stack_pop (q));

        queue_delete (q);

        return 0;
}
#endif
