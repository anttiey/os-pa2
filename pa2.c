/**********************************************************************
 * Copyright (c) 2019-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;


/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;


/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	
	struct process *next = NULL;

	struct process *temp = NULL;
	struct process *small = NULL;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		return current;
	}

pick_next:

	if(!list_empty(&readyqueue)) {

		small = list_first_entry(&readyqueue, struct process, list);

		list_for_each_entry(temp, &readyqueue, list) {

			if(small->lifespan > temp->lifespan) {
				small = temp;
			}

		}

		next = small;
		list_del_init(&next->list);

	}
	
	return next;

}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule,		 /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
static struct process *srtf_schedule(void)
{

	struct process *next = NULL;

	struct process *temp = NULL;
	struct process *small = NULL;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if(!list_empty(&readyqueue)) {

		small = list_first_entry(&readyqueue, struct process, list);

		list_for_each_entry(temp, &readyqueue, list) {

			if((small->lifespan - small->age) > (temp->lifespan - temp->age)) {
				small = temp;
			}

		}

		if((current->lifespan - current->age) > (small->lifespan - small->age)) {

			if((current->lifespan - current->age) > 0) {
				list_add_tail(&current->list, &readyqueue);
			}

			next = small;
			list_del_init(&next->list);

			return next;

		}

	} 
	
	if ((current->lifespan - current->age) > 0) {
			return current;
	}

pick_next:

	if(!list_empty(&readyqueue)) {

		small = list_first_entry(&readyqueue, struct process, list);

		list_for_each_entry(temp, &readyqueue, list) {

			if((small->lifespan - small->age) > (temp->lifespan - temp->age)) {
				small = temp;
			}

		}

		next = small;
		list_del_init(&next->list);

	}
	
	return next;

}


struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.schedule = srtf_schedule,
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void) {

	struct process *next = NULL;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if((current->lifespan - current->age) > 0) {
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:

	if (!list_empty(&readyqueue)) {
	
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
		
	}

	return next;

}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.schedule = rr_schedule,
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	
	return false;

}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */

	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	if (!list_empty(&r->waitqueue)) {

		struct process *temp = NULL;
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		list_for_each_entry(temp, &r->waitqueue, list) {
			if(waiter->prio < temp->prio) {
				waiter = temp;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

static struct process *prio_schedule(void) {

	struct process *next = NULL;

	struct process *temp = NULL;
	struct process *high = NULL;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if((current->lifespan - current->age) > 0) {
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:

	if(!list_empty(&readyqueue)) {

		high = list_first_entry(&readyqueue, struct process, list);

		list_for_each_entry(temp, &readyqueue, list) {

			if(high->prio < temp->prio) {
				high = temp;
			}

		}

		next = high;
		list_del_init(&next->list);

	}
	
	return next;

}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = prio_schedule,
};


/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/

static struct process *pa_schedule(void) {

	struct process *next = NULL;

	struct process *temp = NULL;
	struct process *high = NULL;

	if(!list_empty(&readyqueue)) {

		list_for_each_entry(temp, &readyqueue, list) {
			temp->prio += 1;
			if(temp->prio > MAX_PRIO) {
				temp->prio = MAX_PRIO;
			}
		}

	}

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if((current->lifespan - current->age) > 0) {
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:

	if(!list_empty(&readyqueue)) {

		high = list_first_entry(&readyqueue, struct process, list);

		list_for_each_entry(temp, &readyqueue, list) {

			if(high->prio < temp->prio) {
				high = temp;
			}

		}

		next = high;
		list_del_init(&next->list);
		
		next->prio = next->prio_orig;

	}
	
	return next;

}

struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = pa_schedule,
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		/* Boost the priority of process in PCP */
		current->prio = MAX_PRIO;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */
	
	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	
	return false;

}

void pcp_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */

	if(r->owner != current) {
		return;
	}

	assert(r->owner == current);
	current->prio = current->prio_orig;

	/* Un-own this resource */
	r->owner = NULL;

	if (!list_empty(&r->waitqueue)) {

		struct process *temp = NULL;
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);
		
		list_for_each_entry(temp, &r->waitqueue, list) {
			if(waiter->prio < temp->prio) {
				waiter = temp;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire,
	.release = pcp_release,
	.schedule = prio_schedule,
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */
	if(r->owner->prio < current->prio) {
		r->owner->prio = current->prio;
	} 

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	
	return false;

}

void pip_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */

	r->owner->prio = r->owner->prio_orig;

	if(r->owner != current) {
		return;
	} 
	
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	if (!list_empty(&r->waitqueue)) {

		struct process *temp = NULL;
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);
		
		list_for_each_entry(temp, &r->waitqueue, list) {
			if(waiter->prio < temp->prio) {
				waiter = temp;
			}
		}

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire,
	.release = pip_release,
	.schedule = prio_schedule,
};
