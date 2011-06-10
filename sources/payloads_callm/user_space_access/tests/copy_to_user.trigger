[group]
function.name = copy_to_user

trigger.copy_to_user = yes
trigger.copy_to_user.buffer_size = 1

trigger.code =>>
    int result;
    //user_area - external pointer to user data, type 'void* __user'
    char buf[1] = {'c'};

    result = copy_to_user(user_area, buf, sizeof(buf));
    printk(KERN_DEBUG "[Test] copy_to_user: %d\n", result);
<<
