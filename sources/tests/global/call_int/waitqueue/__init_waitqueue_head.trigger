[group]
function.name = __init_waitqueue_head
trigger.code =>>
    wait_queue_head_t q;
    init_waitqueue_head(&q);
<<
