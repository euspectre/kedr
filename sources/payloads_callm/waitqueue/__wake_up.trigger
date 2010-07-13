wait_queue_head_t q;
init_waitqueue_head(&q);
wake_up(&q);