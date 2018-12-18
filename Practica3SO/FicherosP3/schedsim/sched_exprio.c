#include "sched.h"

/*Seleccionamos una tarea de la run queue, la eliminamos de la run queue y devolvemos. E
 * Esta función es la que se encarga de esta planificación*/
static task_t* pick_next_task_exprio(runqueue_t* rq) {

	task_t* t = head_slist(&rq->tasks); //Escoger una tarea de la runqueue

	if (t)
		remove_slist(&rq->tasks, t); // Sacar dicha tarea de la runqueue

	return t; // Devolver dicha tarea
}

static int compare_tasks_cpu_burst(void *t1,void *t2)
{
	// 1 y 2 tarea
	task_t* tsk1=(task_t*)t1;
	task_t* tsk2=(task_t*)t2;
	// Devolvemos la comparacion.
	return tsk1->prio-tsk2->prio;
}

static void enqueue_task_exprio(task_t* t, runqueue_t* rq, int preempted) {

	rq = get_runqueue_cpu(rq->cpu_rq);

	// Si la tarea a ejecutarse esta en la run queue o lista para ejecutarse entonces salimos.
	if (t->on_rq || is_idle_task(t))
		return;

	if (t->flags & TF_INSERT_FRONT) {
		t->flags&=~TF_INSERT_FRONT;
		sorted_insert_slist_front(&rq->tasks, t, 1, compare_tasks_cpu_burst);
	}else
		sorted_insert_slist(&rq->tasks, t, 1, compare_tasks_cpu_burst);

	if(!preempted) {

		task_t* tareaActual=rq->cur_task;

		// Si CPU es mas pequeño que el actual entonces cambiamos de tarea por una de igual cpu
		if(preemptive_scheduler && !is_idle_task(tareaActual) && (t->prio == 0)) {

			rq->need_resched = TRUE;
			tareaActual->flags|=TF_INSERT_FRONT;
		}
	}
}

static task_t* steal_task_exprio(runqueue_t* rq) {
	task_t* t = tail_slist(&rq->tasks);

	if (t) {
		remove_slist(&rq->tasks, t);
		t->on_rq=FALSE;
		rq->nr_runnable--;
	}
	return t;
}

sched_class_t exprio_sched = {

		.pick_next_task = pick_next_task_exprio,
		.enqueue_task = enqueue_task_exprio,
		.steal_task = steal_task_exprio
};
