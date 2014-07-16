message("Running depmod...")
set(ENV{PATH} "$ENV{PATH}:/sbin:/bin:/usr/bin")
execute_process(COMMAND depmod)
message("Done")