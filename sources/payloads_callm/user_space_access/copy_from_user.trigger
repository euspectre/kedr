int result;
//user_area - external pointer to user data, type 'void* __user'
char buf[1];

result = copy_from_user(buf, user_area, sizeof(buf));