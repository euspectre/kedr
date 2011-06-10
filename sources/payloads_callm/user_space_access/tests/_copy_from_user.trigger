[group]
function.name = _copy_from_user

trigger.copy_from_user = yes
trigger.copy_from_user.buffer_size = 1

trigger.code =>>
    int result;
    //user_area - external pointer to user data, type 'void* __user'
    char buf[1];

    result = copy_from_user(buf, user_area, sizeof(buf));
    printk(KERN_DEBUG "[Test] copy_from_user: %d\n", result);
<<
