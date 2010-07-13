wait_queue_head_t q;
wait_queue_t wait;

init_waitqueue_head(&q);
init_wait(&wait);

prepare_to_wait(&q, &wait, TASK_RUNNING);
finish_wait(&q, &wait);