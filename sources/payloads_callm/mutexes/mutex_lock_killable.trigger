struct mutex m;
mutex_init(&m);
if(!mutex_lock_killable(&m))
	mutex_unlock(&m);
mutex_destroy(&m);