[group]
function.name = __wake_up
trigger.code =>>
    wait_queue_head_t q;
    init_waitqueue_head(&q);
    wake_up(&q);
<<
