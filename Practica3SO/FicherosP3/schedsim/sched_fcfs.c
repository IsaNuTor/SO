#include "sched.h"

static task_t* pick_next_task_fcfs(runqueue_t* rq) {

	task_t* t = head_slist(&rq->tasks); //Escoger una tarea de la runqueue

	/* Cufcfsent is not on the rq -> let's remove it */
	if (t) {
		remove_slist(&rq->tasks, t); // Sacar dicha tarea de la runqueue
		t->on_rq = FALSE;
		// Le asignamos a tarea actual la tarea.
		rq->cur_task = t;
	}
	return t; // Devolver dicha tarea
}

static void enqueue_task_fcfs(task_t* t, runqueue_t* rq, int preempted) {

	rq = get_runqueue_cpu(rq->cpu_rq);

	if (t->on_rq || is_idle_task(t))
		return;

	insert_slist(&rq->tasks, t); //Push task
	t->on_rq=TRUE;

	if(!preempted) {
		rq->nr_runnable++;
		t->last_cpu=rq->cpu_rq;
	}
}

static void task_tick_fcfs(runqueue_t* rq) {

	task_t* cufcfsent = rq->cur_task;

	if (is_idle_task(cufcfsent))
		return;

	if (cufcfsent->runnable_ticks_left == 1)
		rq->nr_runnable--;
}

static task_t* steal_task_fcfs(runqueue_t* rq) {
	task_t* t = tail_slist(&rq->tasks);

	if (t) {
		remove_slist(&rq->tasks, t);
		t->on_rq=FALSE;
		rq->nr_runnable--;
	}
	return t;
}

sched_class_t fcfs_sched = {

		.pick_next_task = pick_next_task_fcfs,
		.enqueue_task = enqueue_task_fcfs,
		.task_tick = task_tick_fcfs,
		.steal_task = steal_task_fcfs
};
