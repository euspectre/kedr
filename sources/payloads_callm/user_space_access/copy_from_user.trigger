int result;
void* user_area = arg;//arg - external variable
char buf[1];

result = copy_from_user(buf, user_area, sizeof(buf));