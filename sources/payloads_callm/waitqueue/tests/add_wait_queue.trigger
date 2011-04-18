[group]
function.name = add_wait_queue
trigger.code =>>
    wait_queue_head_t q;
    wait_queue_t wait;

    init_waitqueue_head(&q);
    init_wait(&wait);

    add_wait_queue(&q, &wait);
    remove_wait_queue(&q, &wait);
<<
