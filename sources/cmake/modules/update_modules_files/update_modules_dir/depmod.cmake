# Load enviroment variable into local one.
# So it can be used in 'if' command.
set(DESTDIR "$ENV{DESTDIR}")

# DESTDIR changes location of installed files, including kernel modules.
# So, no sence to run depmod in that case.
if(DESTDIR)
    return()
endif(DESTDIR)

message("Running depmod...")
set(ENV{PATH} "$ENV{PATH}:/sbin:/bin:/usr/bin")
execute_process(COMMAND depmod)
message("Done")
