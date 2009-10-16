/*
   queue.h: An efficient double-ended queue implementation, header for queue.c

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

#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

struct queue;

struct queue *queue_new (void);
void queue_push (struct queue *q, void *data);
bool queue_empty (struct queue *q);
int queue_len (struct queue *q);
void *queue_pop (struct queue *q);
void *queue_peek (struct queue *q, int i);
void queue_delete (struct queue *q);

/* Stacks and Queues use exactly the same data structure.  In fact, it
 * is legal to interleave stack_pop() and queue_pop() operations if
 * you need a queue where you can pop off of both ends */

#define stack queue
#define stack_new() (queue_new ())
#define stack_push(q,data) (queue_push ((q), (data)))
#define stack_empty(q) (queue_empty ((q)))
#define stack_len(q) (queue_len ((q)))
void *stack_pop (struct stack *q);
void *stack_peek (struct stack *q, int i);
#define stack_delete(q) (queue_delete ((q)))

#endif
