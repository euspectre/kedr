#define _GNU_SOURCE /*need for WEXITSTATUS and WEXITED macros*/

#include <stdio.h> /* printf-like functions */
#include <unistd.h> /* open */
#include <stdlib.h> /* exit, strerror */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h> /* signals processing */
#include <sys/signalfd.h> /* signalfd() */

#include <poll.h> /* poll() */
#include <errno.h>

#include <string.h> /*memset, strdup*/

#include <getopt.h> /* getopt_long() */
 
#include <assert.h> /* assert() */
//Relative path of trace file in the debugfs
#define REL_TRACEFILE "kedr_tracing/trace"
//Default mount point for debugfs
#define DEFAULT_DEBUGFS_MOUNT_POINT "/sys/kernel/debug"

/*
 * Column number in trace line, which represent message
 * (without additional info).
 *
 * Columns should be delimited by '\t'.
 */

#define MESSAGE_COLUMN_NUMBER 4

/*
 * Size of the buffer for reading data from the trace file.
 * 
 * It is better for this buffer to be able to contain full line.
 */
#define READ_BUFFER_SIZE 100

#define PROGRAM_NAME "kedr_capture_trace"

#define print_error(format, ...) fprintf(stderr, "%s: "format"\n", PROGRAM_NAME, __VA_ARGS__)
#define print_error0(error_msg) print_error("%s", error_msg)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({                      \
         const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
         (type *)( (char *)__mptr - offsetof(type,member) );})


enum capture_trace_flags
{
    capture_trace_blocking  = 1,
    capture_trace_session   = 2,
};

static const char usage_message[] = "Usage:\n"
    "\t"PROGRAM_NAME" [options]\n"
    "\n"
    " Read messages from the trace.\n"
    "\n"
    "Options:\n"
    "\t-d <directory>\n"
    "\t\t Assume debugfs filesystem is mounted to the specified directory.\n"
    "\t\tWithout this option debugfs filesystem is assumed to be mounted\n"
    "\t\tto '/sys/kernel/debug'.\n"
    "\t-b, --blocking\n"
    "\t\t Do not stop capturing when no messages left in the trace.\n"
    "\t\tInstead, wait until new messages appear.\n"
    "\t\tThis is similar to the blocking read of the file.\n"
    "\t\tSending SIGINT signal to the process, represented this program\n"
    "\t\t(or pressing 'Ctrl + C' in the shell) cancel this flag,\n"
    "\t\tso program will end when no messages left in the trace.\n"
    "\t-s, --session\n"
    "\t\t Read only those messages from the trace, that were collected from the\n"
    "\t\tmoment when target module was loaded to the moment when target module\n"
    "\t\twas unloaded. After the last message is read, stop capturing.\n"
    "\t-f, --file <file>\n"
    "\t\t Write all messages from the trace to the specified file.\n"
    "\t\tFilename '-' means STDOUT.\n"
    "\t-p, --program <program>\n"
    "\t\t Run the specified program and write all messages from the trace\n"
    "\t\tto the program's STDIN.\n"
    "\n"
    "If neither '-f' nor '-p' option is specified, all messages from the trace\n"
    "will be written to STDOUT. Several options '-f' and/or '-p' are allowed.\n"
    "\n"
    "\t"PROGRAM_NAME" --help\n"
    "\t\t Print this information and exit.\n"
    "\n";
    
/*
 * Barrier for determine whether trace reading should be stoped.
 */

struct trace_barrier
{
    /*
     * Callback function for determine whether
     * it is need to stop capturing trace.
     * 
     * Return 0 if capturing may be continued.
     * 
     * Return 1 if capturing should be stopped.
     * 
     * Return -1 on error.
     */
    int (*func)(const void* trace_data, size_t trace_data_size,
        struct trace_barrier* barrier);
    /*
     * Used to free data of the barrier.
     */
    void (*free)(struct trace_barrier* barrier);
};

/* Barrier for target module session. */
struct target_session_barrier
{
    /* Base barrier structure */
    struct trace_barrier barrier;
    /* Symbols which has read from the current line at the moment. */
    char* current_line;
    size_t current_line_size;
    /* Counter of non-closed markers */
    int markers_counter;
};

static void
target_session_barrier_init(struct target_session_barrier* barrier);

/*
 * Describe consumer of the trace.
 * 
 * This is either process which should accept trace lines via STDIN,
 * or file for copiing trace lines.
 * 
 * In both cases, consuming is performed via file 'fd_write'.
 * 
 * As special case of file for copiing trace - STDOUT.
 */

struct trace_consumer
{
    /*
     * File descriptor for consume trace lines.
     * 
     * -1 if file is closed.
     */
    int fd_write;
    /*
     * Pid of the process or 0 for simple file.
     */
    pid_t pid;
    /*
     * List organization.
     */
    struct trace_consumer* next;
    struct trace_consumer* prev;
};

/*
 * Create child process, redirect its input,
 * and run 'command_line' in it as in the shell.
 * 
 * On error return NULL.
 */

static struct trace_consumer*
trace_consumer_create_process(const char* command_line);

/*
 * Create descriptor, wia which one can write into the file.
 * 
 * Filename "-" means STDOUT.
 * 
 * On error return NULL.
 */

static struct trace_consumer*
trace_consumer_create_file(const char* filename);

/*
 * Destroy trace consumer.
 * 
 * NOTE: waiting for child process is not performed.
 */
static void trace_consumer_free(struct trace_consumer* child);

/*
 * Helper for close file via which consume process is performed.
 */
static void trace_consumer_stop_write(struct trace_consumer* child);

/*
 * Helper for testing, consume process is active.
 */
static int trace_consumer_is_writeable(struct trace_consumer* child);

/*
 * List of trace_consumers.
 * 
 * Unordered(in that sence, that order has no sence).
 */
struct trace_consumers
{
    struct trace_consumer* first_consumer;
    struct trace_consumer* last_consumer;

    struct trace_barrier* barrier;
};

/*
 * Initialize list of trace_consumers as empty and without barrier.
 */
static int
trace_consumers_init(struct trace_consumers* trace_consumers);

/* Set barrier for consumers. */
static void
trace_consumers_set_barrier(struct trace_consumers* trace_consumers,
    struct trace_barrier* barrier);

/* Add consumer to the tail of the list. */
static void trace_consumers_add_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer);

/* Remove consumer from the list. */
static void
trace_consumers_del_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer);

/* Macro for iterate consumers in the list */
#define trace_consumers_for_each(trace_consumers, consumer) \
    for((consumer) = (trace_consumers)->first_consumer; (consumer) != NULL; (consumer) = (consumer)->next)

/*
 * Free and remove all consumers from trace_consumers.
 * 
 * Waiting is not performed.
 */
static void
trace_consumers_free(struct trace_consumers* trace_consumers);

/*
 * Free and remove all consumers from trace_consumers.
 * 
 * Also wait, while every child is finished.
 * 
 * Return 0, if every child exit with 0 status.
 * Otherwise return 1.
 */
static int
trace_consumers_free_wait(struct trace_consumers* trace_consumers);

/*
 * Consume data from the trace.
 * 
 * Return 0 on success.
 * 
 * Return -1 on fail.
 * 
 * Return 1, if data has consumed,
 * but reading of the trace should be stopped(barrier triggered).
 */
static int
trace_consumers_process_data(struct trace_consumers* trace_consumers,
    const void* trace_data, size_t count);


/*
 * Auxiliary function for change flags for file descriptor.
 * 
 * Return 0 on success.
 */
static int change_fd_flags(int fd, long mask, long value);

/*
 * Auxiliary function for change status flags for file.
 * 
 * Return 0 on success.
 */

static int change_fl_flags(int fd, long mask, long value);


/*
 * Return file descriptor,
 * which should be used for every operation which may block.
 * (so, every blocking read/write should be implemented with
 * poll/select + non-blocking read/write).
 * 
 * For non-blocking mode return -1.
 */

static int blocking_mode_get_fd(void);

/*
 * Function which should be called after successfull polling
 * of the file descriptor, returned by blocking_mode_get_fd().
 */

static void blocking_mode_update(void);

/*
 * Initialize blocking mode of capture trace program.
 * 
 * In that mode reading of the trace may block, but first emission of
 * SIGINT signal should turn blocking mode off.
 * 
 * Such emulation of SIGINT signal behaviour require to call
 * get_blocking_fd() before every operation(not only operation for reading trace),
 * which may block, and waiting on the returned file descriptor.
 * Otherwise, even additional SIGNINT do not break waiting.
 *
 */
static int blocking_mode_set(void);

/* Should be called when blocking mode support is not longer needed.*/
static void blocking_mode_clear(void);

/*
 * Envelop around read for reading trace file.
 *
 * Correctly process both blocking and non-blocking modes.
 * 
 * Note: fd_trace should be opened in non-blocking mode.
 */
static ssize_t
trace_read_common(int fd_trace, void* buf, size_t count);

/*
 * Envelop around write() to some file.
 *
 * Whenever blocking flag of the program is set or not,
 * writing is perfromed effectivly in blocking mode.
 *
 * The thing is that, simple blocking writing is not correct,
 * because in that case program will be insensitive to ALL
 * SIGINT signals, but should be immune only to first one.
 * 
 * Note: fd should be opened in non-blocking mode.
 */
static ssize_t
write_blocking(int fd, const void* buf, size_t count);

/*
 * Process options of the program and collect value of them
 * in the corresponded variables.
 * 
 * Return 0 on success.
 */
static int process_options(int argc, char* const argv[],
// output variables for collect options values
    //
    enum capture_trace_flags *flags, 
    //
    const char** debugfs_mount_point,
    //NULL-terminated array of file names, should be freed after using.
    const char*** file_names,
    //NULL-terminated array of program names, should be freed after using.
    const char*** program_names
);

/* Helper for main */
static size_t snprintf_trace_filename(char* dest, size_t size,
    const char* debugfs_mount_point)
{
    return snprintf(dest, size,
        "%s/%s", debugfs_mount_point, REL_TRACEFILE);
}

/*
 * Main.
 */

int main(int argc, char* const argv[], char* const envp[])
{
    int fd_trace;
    int result = 0;
    
    enum capture_trace_flags flags;
    const char* debugfs_mount_point;
    char const** file_names;
    char const** program_names;
    
    result = process_options(argc, argv,
        &flags,
        &debugfs_mount_point,
        &file_names,
        &program_names);
    if(result)
        return result;

    size_t trace_file_name_len = snprintf_trace_filename(NULL, 0,
        debugfs_mount_point);
    char* trace_file_name = malloc(trace_file_name_len + 1);
    if(trace_file_name == NULL)
    {
        print_error0("Cannot allocate memory for trace file name.");
        free(program_names);
        free(file_names);
    }
    snprintf_trace_filename(trace_file_name, trace_file_name_len + 1,
        debugfs_mount_point);
    
    fd_trace = open(trace_file_name, O_RDONLY);
    if(fd_trace == -1)
    {
        switch(errno)
        {
        case ENOENT:
            print_error("Trace file '%s' does not exist.\n"
                "Debugfs is probably not mounted to \"%s\".",
                trace_file_name, debugfs_mount_point);
        break;
        case EBUSY:
            print_error("Trace file is busy.\n"
                "Another instance of %s is probably processing it.",
                PROGRAM_NAME);
        break;
        //case EACCESS: what message should be here?
        default:
            print_error("Cannot open trace file '%s' for reading: %s.",
                trace_file_name, strerror(errno));
        }
        free(trace_file_name);
        free(program_names);
        free(file_names);
        return -1;
    }
    free(trace_file_name);
    
    if(change_fd_flags(fd_trace, FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for opened trace file.");
        close(fd_trace);
        free(program_names);
        free(file_names);
        return -1;
    }
    
    if(change_fl_flags(fd_trace, O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for opened trace file.");
        close(fd_trace);
        free(program_names);
        free(file_names);
        return -1;
    }

    struct trace_consumers trace_consumers;
    trace_consumers_init(&trace_consumers);
    if(flags & capture_trace_session)
    {
        static struct target_session_barrier barrier;
        target_session_barrier_init(&barrier);
        trace_consumers_set_barrier(&trace_consumers, &barrier.barrier);
    }
    
    const char** program_name;
    for(program_name = program_names; *program_name != NULL; program_name++)
    {
        // Create another process which piped with current
        struct trace_consumer* consumer =
            trace_consumer_create_process(*program_name);
        if(consumer == NULL)
        {
            print_error("Cannot create child process \"%s\".",
                *program_name);
            trace_consumers_free(&trace_consumers);
            close(fd_trace);
            free(program_names);
            free(file_names);
            return -1;
        }
        trace_consumers_add_consumer(&trace_consumers, consumer);
    }
    free(program_names);
    
    const char** file_name;
    for(file_name = file_names; *file_name != NULL; file_name++)
    {
        // Create trace consumer as file
        struct trace_consumer* consumer =
            trace_consumer_create_file(*file_name);
        if(consumer == NULL)
        {
            print_error("Cannot open file for writing trace \"%s\".",
                *file_name);
            trace_consumers_free(&trace_consumers);
            close(fd_trace);
            free(file_names);
            return -1;
        }
        trace_consumers_add_consumer(&trace_consumers, consumer);
    }
    free(file_names);
    
    if(flags & capture_trace_blocking)
    {
        if(blocking_mode_set())
        {
            trace_consumers_free(&trace_consumers);
            close(fd_trace);
            return -1;
        }
    }

    /* Read trace until error occures or should stop for other reasons.*/
    do
    {
        char trace_buffer[READ_BUFFER_SIZE];
        ssize_t read_size;
        
        read_size = trace_read_common(fd_trace,
            trace_buffer, sizeof(trace_buffer));
        
        if(read_size == -1)
        {
            print_error("Error occures while reading trace: %s.",
                strerror(errno));
            result = -1;
            break;
        }
        else if(read_size == 0)
        {
            /* EOF */
            result = 1;
            break;
        }
        
        result = trace_consumers_process_data(&trace_consumers,
            trace_buffer, read_size);

    }while(result == 0);

    if(flags & capture_trace_blocking)
    {
        blocking_mode_clear();
    }

    close(fd_trace);
    
    if(result == -1)
    {
        trace_consumers_free(&trace_consumers);
        return -1;
    }
    else
        return trace_consumers_free_wait(&trace_consumers);
}

/*
 * Implementation of auxiliary functions.
 */

int change_fd_flags(int fd, long mask, long value)
{
    long flags = fcntl(fd, F_GETFD);
    if(flags == -1)
    {
        print_error("Cannot get flags for file descriptor: %s.",
            strerror(errno));
        return -1;
    }
    flags = (flags & ~mask) | (value & mask);
    
    if(fcntl(fd, F_SETFD, flags) == -1)
    {
        print_error("Cannot set flags to file descriptor: %s.",
            strerror(errno));
        return -1;
    }
    return 0;
}

int change_fl_flags(int fd, long mask, long value)
{
    long flags = fcntl(fd, F_GETFL);
    if(flags == -1)
    {
        print_error("Cannot get status flags for file: %s.",
            strerror(errno));
        return -1;
    }
    flags = (flags & ~mask) | (value & mask);
    
    if(fcntl(fd, F_SETFL, flags) == -1)
    {
        print_error("Cannot set status flags to file: %s.",
            strerror(errno));
        return -1;
    }
    return 0;
}

static ssize_t
trace_read_nonblock(int fd_trace, void* buf, size_t count)
{
    /* Simple non-blocking read of the trace */
    ssize_t result;
    /* Repeat reading while it is interrupted*/
    do
    {
        result = read(fd_trace, buf, count);
    }while((result == -1) && (errno == EINTR));

    if((result == -1) && (errno == EAGAIN))
    {
        /* "Reading would block" = EOF in non-blocking mode */
        result = 0;
    }

    return result;
}

ssize_t trace_read_common(int fd_trace, void* buf, size_t count)
{
    int blocking_mode_fd = blocking_mode_get_fd();
    if(blocking_mode_fd == -1)
    {
        return trace_read_nonblock(fd_trace, buf, count);
    }
    else
    {
        /* Blocking mode, but also need to polling special file descriptor. */
        ssize_t result;
        struct pollfd pollfds[2] = {
            {
                .fd = -1,//will be fd_trace
                .events = POLLIN,
            },
            {
                .fd = -1,//will be fd of blocking_mode implementation
                .events = POLLIN
            }
        };
        pollfds[0].fd = fd_trace;
        pollfds[1].fd = blocking_mode_fd;
        /* Repeat poll()+read() until some data has read or error occured.*/
        do
        {
            // Repeate poll() while interrupted with non-interesting signals
            do
            {
                result = poll(pollfds, 2, -1/*forever*/);
                //printf("poll exited.\n");
            }while((result == -1) && (errno == EINTR));

            if(result == -1)
            {
                print_error("poll() fail: %s.", strerror(errno));
                return -1;
            }
            else if(result == 0)
            {
                print_error0("Unexpected return value 0 from poll() without timeout.");
                return -1;
            }
            /* Successfull poll */
            if(pollfds[1].revents & POLLIN)
            {
                /* Signal which should set non-blocking mode has arrived */
                blocking_mode_update();
                assert(blocking_mode_get_fd() == -1);
                /* Now mode should be non-blocking. */
                return trace_read_nonblock(fd_trace, buf, count);
            }
            /* new data become available in the trace */
            result = read(fd_trace, buf, count);
        }while((result == -1) && ((errno == EAGAIN) || (errno == EINTR)));
        
        return result;
    }
}

ssize_t write_blocking(int fd, const void* buf, size_t count)
{
    int blocking_mode_fd = blocking_mode_get_fd();

    ssize_t result;
    struct pollfd pollfds[2] = {
        {
            .fd = -1,//will be fd
            .events = POLLOUT,
        },
        {
            .fd = -1,//will be fd of blocking_mode implementation
            .events = POLLIN
        }
    };
    pollfds[0].fd = fd;

    if(blocking_mode_fd != -1)
    {
        /* Need also to poll blocking mode file descriptor*/
        pollfds[1].fd = blocking_mode_fd;
    }

    do
    {
        // Repeate poll() while interrupted with non-interesting signals
        do
        {
            result = poll(pollfds, (blocking_mode_fd != -1) ? 2 : 1,
                -1/*forever*/);
            //printf("poll exited.\n");
        }while((result == -1) && (errno == EINTR));

        if(result == -1)
        {
            print_error("poll() fail: %s.", strerror(errno));
            return -1;
        }
        else if(result == 0)
        {
            print_error0("Unexpected return value 0 from poll() without timeout.");
            return -1;
        }
        /* Successfull poll */
        if((blocking_mode_fd != -1) && (pollfds[1].revents & POLLIN))
        {
            /* Signal which should set non-blocking mode has arrived */
            blocking_mode_update();
            assert(blocking_mode_get_fd() == -1);
            /* Now mode should be non-blocking - simply call function again*/
            return write_blocking(fd, buf, count);
        }
        /* file may be written without block */
        result = write(fd, buf, count);
    }while((result == -1) && ((errno == EAGAIN) || (errno == EINTR)));

    return result;
}

struct trace_consumer*
trace_consumer_create_process(const char* command_line)
{
    struct trace_consumer* consumer;
    pid_t pid;
    int fd_pipe[2];
    //Allocate consumer structure
    consumer = malloc(sizeof(*consumer));
    if(consumer == NULL)
    {
        print_error0("Cannot allocate consumer structure.");
        return NULL;
    }
    /*
     * Set up all fields for correct deleting
     * in case of error in initialization.
     */
    consumer->fd_write = -1;
   
    // Create another process which piped with current
    if(pipe(fd_pipe) == -1)
    {
        print_error("Cannot create pipe: %s.",
            strerror(errno));
        trace_consumer_free(consumer);
        return NULL;
    }
    
    if(change_fd_flags(fd_pipe[1], FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for write end of pipe.");
        close(fd_pipe[0]);
        close(fd_pipe[1]);
        trace_consumer_free(consumer);
        return NULL;
    }
    
    if(change_fl_flags(fd_pipe[1], O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for write end of pipe.");
        close(fd_pipe[0]);
        close(fd_pipe[1]);
        trace_consumer_free(consumer);
        return NULL;
    }

    // do not terminate when child process close its stdin
    signal(SIGPIPE, SIG_IGN);

    pid = fork();
    if(pid == -1)
    {
        print_error("Error when performing fork: %s.", strerror(errno));
        close(fd_pipe[0]);
        close(fd_pipe[1]);
        trace_consumer_free(consumer);
        return NULL;
    }
    if(pid == 0)
    {
        // consumer
        close(fd_pipe[1]);
        if(dup2(fd_pipe[0], 0) == -1)
        {
            print_error("Cannot redirect input of the child process: %s.",
                strerror(errno));
            close(fd_pipe[0]);
            exit(-1);
        }
        close(fd_pipe[0]);
        
        // For some reason system() do not protect consumer process against
        // SIGINT for process group(!).
        signal(SIGINT, SIG_IGN);
        exit(system(command_line));

    }
    //Parent
    close(fd_pipe[0]);
    //
    consumer->pid = pid;
    consumer->fd_write = fd_pipe[1];
    
    return consumer;
}

/*
 * Create descriptor, via which one can write into the file
 * 
 * On error return NULL.
 * 
 * 'types' represent types of trace lines, which accepted by this file.
 */

struct trace_consumer*
trace_consumer_create_file(const char* filename)
{
    struct trace_consumer* consumer;
    int fd;
    //Allocate consumer structure
    consumer = malloc(sizeof(*consumer));
    if(consumer == NULL)
    {
        print_error0("Cannot allocate consumer structure.");
        return NULL;
    }
    /*
     * Set up all fields for correct deleting
     * in case of error in initialization.
     */
    consumer->pid = 0;
    consumer->fd_write = -1;
    // Open file for writing trace.
    if(strcmp(filename, "-") != 0)
    {
        fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd == -1)
        {
            print_error("Cannot open file \"%s\" for collecting trace: %s.",
                filename, strerror(errno));
            free(consumer);
            return NULL;
        }
    }
    else
    {
        fd = dup(1);
        if(fd == -1)
        {
            print_error("Cannot duplicate STDOUT: %s.",
                strerror(errno));
            free(consumer);
            return NULL;
        }

    }

    if(change_fl_flags(fd, O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for file for collecting trace.");
        close(fd);
        free(consumer);
        return NULL;
    }


    if(change_fd_flags(fd, FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for file for collecting trace.");
        close(fd);
        free(consumer);
        return NULL;
    }
    
    consumer->fd_write = fd;
    
    return consumer;
}


void trace_consumer_free(struct trace_consumer* consumer)
{
    if(consumer == NULL) return;
    
    trace_consumer_stop_write(consumer);
    free(consumer);
}

void trace_consumer_stop_write(struct trace_consumer* consumer)
{
    if(consumer->fd_write != -1)
    {
        close(consumer->fd_write);
        consumer->fd_write = -1;
    }
}

int trace_consumer_is_writeable(struct trace_consumer* consumer)
{
    return consumer->fd_write != -1;
}

int trace_consumers_init(struct trace_consumers* trace_consumers)
{
    trace_consumers->first_consumer = NULL;
    trace_consumers->last_consumer = NULL;
    
    trace_consumers->barrier = NULL;
}

void trace_consumers_set_barrier(struct trace_consumers* trace_consumers,
    struct trace_barrier* barrier)
{
    trace_consumers->barrier = barrier;
}

void trace_consumers_add_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer)
{
    struct trace_consumer* last_consumer = trace_consumers->last_consumer;
    if(last_consumer != NULL)
    {
        last_consumer->next = consumer;
        consumer->prev = last_consumer;
    }
    else
    {
        trace_consumers->first_consumer = consumer;
        consumer->prev = NULL;
    }
    consumer->next = NULL;
    trace_consumers->last_consumer = consumer;
}

void
trace_consumers_del_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer)
{
    if(consumer->next)
        consumer->next->prev = consumer->prev;
    else
        trace_consumers->last_consumer = consumer->prev;
    if(consumer->prev)
        consumer->prev->next = consumer->next;
    else
        trace_consumers->first_consumer = consumer->next;

    consumer->next = NULL;
    consumer->prev = NULL;
}

void trace_consumers_free(struct trace_consumers* trace_consumers)
{
    struct trace_consumer* consumer;
    while((consumer = trace_consumers->first_consumer) != NULL)
    {
        trace_consumers_del_consumer(trace_consumers, consumer);
        trace_consumer_free(consumer);
    }
    
    struct trace_barrier* barrier = trace_consumers->barrier;
    if(barrier != NULL)
        if(barrier->free != NULL) barrier->free(barrier);
}

int trace_consumers_free_wait(struct trace_consumers* trace_consumers)
{
    int result = 0;
    struct trace_consumer* consumer;
    
    trace_consumers_for_each(trace_consumers, consumer)
    {
        trace_consumer_stop_write(consumer);
    }
    
    struct trace_barrier* barrier = trace_consumers->barrier;
    if(barrier != NULL)
        if(barrier->free != NULL) barrier->free(barrier);

    while((consumer = trace_consumers->first_consumer) != NULL)
    {
        int status;
        pid_t pid = consumer->pid;
        trace_consumers_del_consumer(trace_consumers, consumer);
        
        if(pid != 0)
        {
            //printf("Wait for child %d...\n", (int)pid);
            waitpid(pid, &status, 0);
            
            if(!WIFEXITED(status) || WEXITSTATUS(status))
                result = 1;
        }
        //printf("Child %d has finished.\n", (int)pid);
        trace_consumer_free(consumer);
    }
    return result;
}


/* Implementation of blocking mode of the program */
static int fd_signal = -1;

int blocking_mode_get_fd(void)
{
    return fd_signal;
}

int blocking_mode_set(void)
{
    int fd_signal_tmp;
    sigset_t signal_mask;
    
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    
    fd_signal_tmp = signalfd(-1, &signal_mask, 0);
    if(fd_signal_tmp == -1)
    {
        print_error("Cannot create file descriptor for signal SIGINT: %s.",
            strerror(errno));
        return -1;
    }
    if(change_fd_flags(fd_signal_tmp, FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for signal file descriptor.");
        close(fd_signal_tmp);
        return -1;
    }
    if(change_fl_flags(fd_signal_tmp, O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for signal file descriptor.");
        close(fd_signal_tmp);
        return -1;
    }

    //Otherwise signal will not get into fd_signal
    sigprocmask(SIG_BLOCK, &signal_mask, NULL);
    fd_signal = fd_signal_tmp;
    return 0;
}

void blocking_mode_clear(void)
{
    if(fd_signal != -1)
    {
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGINT);
        
        sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
        close(fd_signal);
        fd_signal = -1;
    }
    
}

void blocking_mode_update(void)
{
    if(fd_signal != -1)
    {
        /*
         *  Consume signal in the signal file descriptor.
         *  Otherwise, it will 'alive', when be unblocked.
         */

        ssize_t result;
        struct signalfd_siginfo siginfo;
        /* Repeat read while interrupted */
        do
        {
            result = read(fd_signal, &siginfo, sizeof(siginfo));
        }while((result == -1) && (errno == EINTR));
        
        if(result > 0)
        {
            /* Signal was arrived. Set mode to non-block. */
            sigset_t signal_mask;
            sigemptyset(&signal_mask);
            sigaddset(&signal_mask, SIGINT);
            sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
            close(fd_signal);
            fd_signal = -1;
        }
    }
}


/*
 * Same as write_blocking(), but repeat write until all bytes will be written
 * or real error occurs.
 */
static ssize_t
write_blocking_all(int fd, const void* buffer, size_t size)
{
    size_t bytes_written = 0;
    /* Repeat write until all data will be written or error occures*/
    do
    {
        ssize_t result = write_blocking(fd, buffer, size);
        if(result == -1) return -1;

        bytes_written += result;
        buffer += result;
        size -= result;
    } while(size != 0);
    return bytes_written;
}


int
trace_consumers_process_data(struct trace_consumers *trace_consumers,
    const void* buf, size_t count)
{
    struct trace_consumer *consumer;

    trace_consumers_for_each(trace_consumers, consumer)
    {
        if(!trace_consumer_is_writeable(consumer)) continue;
        if(write_blocking_all(consumer->fd_write, buf, count) == -1)
        {
            if((errno == EPIPE) && (consumer->pid != 0))
            {
                print_error0("Child process has closed its STDIN.");
            }
            else
            {
                print_error("Error occured while writing to the pipe with child process: %s.\n"
                    "Writing to this process will stop.",
                    strerror(errno));
            }
            trace_consumer_stop_write(consumer);
        }
    }
   
    struct trace_barrier* barrier = trace_consumers->barrier;
    if(barrier != NULL)
    {
        if(barrier->func(buf, count, barrier))
        {
            // Need to stop reading(barrier or error)
            return 1;
        }
    }
    
    return 0;

}

/* 
 * Collect full trace line for barrier(helper).
 *
 * Accoring to current state of barrier and content of str and str_size
 * (readed data from the trace)
 * return 1 and set line to full line, or return 0 and update barrier state.
 * On error, return -1.
 */

static int
collect_trace_line(struct target_session_barrier* barrier,
    const char* str, size_t str_size,
    const char** line, size_t* line_size)
{
    if(barrier->current_line != NULL)
    {
        /*
         * Trace data are continuation of the previously collected line.
         * So, concat data.
         */
        char* current_line = realloc(barrier->current_line,
            barrier->current_line_size + str_size);
        if(current_line == NULL)
        {
            print_error0("Failed to extend buffer for trace line.");
            return -1;
        }
        memcpy(current_line + barrier->current_line_size,
            str, str_size);
        
        barrier->current_line = current_line;
        barrier->current_line_size += str_size;

        if(barrier->current_line[barrier->current_line_size - 1] != '\n')
        {
            //printf("Collect another line(size is %zu) ...\n", str_size);
            return 0;//full trace line has not collect yet
        }
        *line = barrier->current_line;
        *line_size = barrier->current_line_size;
        
        return 1;
    }
    else if(str[str_size - 1] != '\n')
    {
        /*
         * Trace data represent only beginning of the trace line.
         * Store them and return.
         */
        barrier->current_line = malloc(str_size);
        if(barrier->current_line == NULL)
        {
            print_error0("Failed to allocate buffer for trace line.");
            return -1;
        }
        
        memcpy(barrier->current_line, str, str_size);
        barrier->current_line_size = str_size;
        printf("Collect first line(size is %zu) ...\n", str_size);
        return 0;
    }
    else
    {
        /*
         * Trace data represent full trace line.
         */
        *line = str;
        *line_size = str_size;
        
        return 1;
    }
}


/* Callback for target session barrier */
static int
target_session_barrier_func(const void* trace_data,
    size_t trace_data_size, struct trace_barrier* barrier)
{
    struct target_session_barrier* barrier_real =
        container_of(barrier, struct target_session_barrier, barrier);

    // Full line of the trace
    const char* trace_line;
    size_t trace_line_size;

    int result = collect_trace_line(barrier_real,
        trace_data, trace_data_size,
        &trace_line, &trace_line_size);
        
    if(result != 1) return result;

    assert(trace_line != NULL);
    assert(trace_line_size != 0);

    // Process full trace line
    static char start_marker[] = "target_session_begins:";
    static char end_marker[] = "target_session_ends:";

    const char* marker;
    
    const char* trace_line_end = trace_line + trace_line_size - 1;//do not process '\n'

#if (MESSAGE_COLUMN_NUMBER > 1)
    int column_number = 1;
    for(marker = trace_line; marker < trace_line_end; ++marker)
    {
        if(*marker == '\t')
        {
            column_number++;
            if(column_number >= MESSAGE_COLUMN_NUMBER)
            {
                marker++;
                break;
            }
        }
    }
#else
    marker = trace_line;
#endif

#define is_substr(str, len, sub_str) \
    ((len >= strlen(sub_str)) && (strncmp(str, sub_str, strlen(sub_str)) == 0))
    
    if(is_substr(marker, trace_line_end - marker, start_marker))
    {
        /* start marker*/
        barrier_real->markers_counter++;
        result = 0;
    }
    else if(barrier_real->markers_counter == 0)
    {
        /* common message without start marker*/
        print_error("Trace line '%.*s' outside target session.",
            (int)(trace_line_end - trace_line), trace_line);
        //only warning
        result = 0;
    }
    else if(is_substr(marker, trace_line_end - marker, end_marker))
    {
        /* end marker */
        barrier_real->markers_counter--;
        result = (barrier_real->markers_counter == 0) ? 1 : 0;
    }
    else
    {
        /* common message*/
        result = 0;
    }
#undef is_substr

    /* Free buffer for line, if needed */
    if(barrier_real->current_line != NULL)
    {
        free(barrier_real->current_line);
        barrier_real->current_line = NULL;
        barrier_real->current_line_size = 0;
    }

    return result;
}

static void
target_session_barrier_free(struct trace_barrier* barrier)
{
    struct target_session_barrier* barrier_real =
        container_of(barrier, struct target_session_barrier, barrier);
    
    free(barrier_real->current_line);
}

void
target_session_barrier_init(struct target_session_barrier* barrier)
{
    barrier->barrier.func = target_session_barrier_func;
    barrier->barrier.free = target_session_barrier_free;
    
    barrier->current_line = NULL;
    barrier->current_line_size = 0;
    barrier->markers_counter = 0;
}

// Available program's options
static const char short_options_h[] = "hsbd:f:p:";
static struct option long_options_h[] = {
    {"help", 0, 0, 'h'},
    {"session", 0, 0, 's'},
    {"blocking", 0, 0, 'b'},
    {"file", 1, 0, 'f'},
    {"program", 1, 0, 'p'},
    {0, 0, 0, 0}
};
//Same but without '-h' and '--help'
static const char* short_options = short_options_h + 1;
static struct option* long_options = long_options_h + 1;

static const char** array_names_init(void)
{
    const char** names = malloc(sizeof(*names));
    if(names == 0) return NULL;
    names[0] = NULL;
    return names;
}
static const char** array_names_add(const char** names, const char* name)
{
    const char** p = names;
    while(*p) ++p;
    *p = name;
    size_t new_size = p - names + 1;
    names = realloc(names, (new_size + 1) * sizeof(*names));
    if(names == NULL) return NULL;
    names[new_size] = NULL;
    return names;
}

static int process_options(int argc, char* const argv[],
// output variables for collect options values
    //
    enum capture_trace_flags* flags, 
    //
    const char** debugfs_mount_point,
    //NULL-terminated array of file names, should be freed after using.
    const char*** file_names,
    //NULL-terminated array of program names, should be freed after using.
    const char*** program_names
)
{
    //variables for reading options
    const char** fnames = NULL;
    const char** pnames = NULL;
    
    int opt;
    
    //
    *flags = 0;
    *debugfs_mount_point = DEFAULT_DEBUGFS_MOUNT_POINT;
    
    opt = getopt_long(argc, argv, short_options_h, long_options_h, NULL);
    if(opt == 'h')
    {
        printf("Program for capturing trace generated by KEDR.\n");
        printf("%s", usage_message);
        return 1;
    }

    fnames = array_names_init();
    if(fnames == NULL)
    {
        print_error0("Cannot allocate memory for options (names of files).");
        return -1;
    }
    
    pnames = array_names_init();
    if(pnames == NULL)
    {
        print_error0("Cannot allocate memory for options (names of programs).");
        free(fnames);
        return -1;
    }

    for(;opt != -1; opt = getopt_long(argc, argv, short_options, long_options,
            NULL))
    {
        switch(opt)
        {
        case '?':
            //error in options
            if(optind < argc)
                print_error("Unknown option '%s'.\n"
                    "Execute 'kedr_capture_trace -h' to see the description of program's parameters.",
                    argv[optind]);
            free(pnames);
            free(fnames);
            return -1;
        case 's':
            *flags |= capture_trace_session;
            break;
        case 'b':
            *flags |= capture_trace_blocking;
            break;
        case 'd':
            *debugfs_mount_point = optarg;
            break;
        case 'f':
            fnames = array_names_add(fnames, optarg);
            if(fnames == NULL)
            {
                print_error0("Cannot allocate memory for options (names of files).");
                free(pnames);
                return -1;
            }
            break;
        case 'p':
            pnames = array_names_add(pnames, optarg);
            if(pnames == NULL)
            {
                print_error0("Cannot allocate memory for options (names of programs).");
                free(fnames);
                return -1;
            }
            break;
        default:
            //strange result from getopt_long
            print_error("getopt_long return strange result: %d.", opt);
            free(pnames);
            free(fnames);
            return -1;
        }
        
    }
    if((fnames[0] == NULL) && (pnames[0] == NULL))
    {
        fnames = array_names_add(fnames, "-");
        if(fnames == NULL)
        {
            print_error0("Cannot allocate memory for options (names of files).");
            free(pnames);
            return -1;
        }
    }
    
    *file_names = fnames;
    *program_names = pnames;
    
    return 0;
}
