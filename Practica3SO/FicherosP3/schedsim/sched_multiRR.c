#include "sched.h"

/* Global RR quantum parameter */
int multiRR_quantum = 3;
int global_max_slice = 5;

/* Structure to store RR thread-specific fields */
struct multiRR_data {
	int remaining_ticks_slice;
	int current_slice;
	int quantumExhausted;
	int maxSlice;

};

static int task_new_multiRR(task_t* t) {
	struct multiRR_data* cs_data = malloc(sizeof(struct multiRR_data));

	if (!cs_data)
		return 1; /* Cannot reserve memory */

	/* initialize the quantum */
	cs_data->remaining_ticks_slice = multiRR_quantum;
	cs_data->current_slice = multiRR_quantum;
	cs_data->quantumExhausted = -1;
	cs_data->maxSlice = global_max_slice;
	t->tcs_data = cs_data;

	return 0;
}

static void task_free_multiRR(task_t* t) {
	if (t->tcs_data) {
		free(t->tcs_data);
		t->tcs_data = NULL;
	}
}

static task_t* pick_next_task_multiRR(runqueue_t* rq) {
	task_t* t = head_slist(&rq->tasks);

	/* Current is not on the rq -> let's remove it */
	if (t)
		remove_slist(&rq->tasks, t);

	return t;
}

static void enqueue_task_multiRR(task_t* t, runqueue_t* rq, int preempted) {
	struct multiRR_data* cs_data = (struct multiRR_data*) t->tcs_data;

	if (t->on_rq || is_idle_task(t))
		return;

	if (cs_data->quantumExhausted == 1 && cs_data->current_slice > 1) {
		cs_data->current_slice--;
	} else if (cs_data->quantumExhausted == 0
			&& cs_data->current_slice < cs_data->maxSlice) {
		cs_data->current_slice++;
	}

	insert_slist(&rq->tasks, t); //Push task
	cs_data->remaining_ticks_slice = cs_data->current_slice; // Reset slice
}

static void task_tick_multiRR(runqueue_t* rq) {
	task_t* current = rq->cur_task;
	struct multiRR_data* cs_data = (struct multiRR_data*) current->tcs_data;

	if (is_idle_task(current))
		return;

	cs_data->remaining_ticks_slice--; /* Charge tick */

	cs_data->quantumExhausted = 0;

	if (cs_data->remaining_ticks_slice <= 0) {
		if (current->runnable_ticks_left > 1) {

			cs_data->quantumExhausted = 1;
		}
		rq->need_resched = TRUE; //Force a resched !!
	}
}

static task_t* steal_task_multiRR(runqueue_t* rq) {
	task_t* t = tail_slist(&rq->tasks);

	if (t)
		remove_slist(&rq->tasks, t);

	return t;
}

sched_class_t multiRR_sched = { .task_new = task_new_multiRR, .task_free =
		task_free_multiRR, .pick_next_task = pick_next_task_multiRR,
		.enqueue_task = enqueue_task_multiRR, .task_tick = task_tick_multiRR,
		.steal_task = steal_task_multiRR };
