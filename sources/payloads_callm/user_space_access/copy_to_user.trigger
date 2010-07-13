int result;
void* user_area = arg;//arg - external variable
char buf[1] = {'c'};

result = copy_to_user(user_area, buf, sizeof(buf));