#define _GNU_SOURCE /*need for WEXITSTATUS and WEXITED macros*/

#include <stdio.h> /*printf*/
#include <unistd.h> /*open*/
#include <stdlib.h> /*exit*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h> /*signals processing*/
#include <sys/signalfd.h> /*signalfd()*/

#include <poll.h> //for poll()
#include <errno.h>

#include <string.h> /*memset, strdup*/

 #include <getopt.h> /* getopt_long() */

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
 * Sizes of the buffer for reading line from the trace file.
 * 
 * At least, READ_BUFFER_SIZE_MIN will be allocated for buffer.
 * If it is insufficient for reading line, buffer will be reallocated
 * with increased size.
 * 
 * If buffer size is less or equal than READ_BUFFER_SIZE_MAX bytes,
 * it may be reused for following reads without freeing (like cache).
 * 
 * If buffer size is greater than READ_BUFFER_SIZE_MAX bytes,
 * then it will be freed after processing line.
 */
#define READ_BUFFER_SIZE_MIN 100
#define READ_BUFFER_SIZE_MAX 500

#define PROGRAM_NAME "kedr_capture_trace"

#define print_error(format, ...) fprintf(stderr, "%s: "format"\n", PROGRAM_NAME, __VA_ARGS__)
#define print_error0(error_msg) print_error("%s", error_msg)

static const char usage_message[] = "Usage:\n"
    "\t"PROGRAM_NAME" [options]\n"
    "\n"
    " Read messages from the trace until stopped by SIGINT signal (Ctrl+C) or\n"
    "until no futher messages should be read (see option '-s' description).\n"
    "\n"
    "Options may be:\n"
    "\t-d <directory>\n"
    "\t\t Assume debugfs filesystem mounted into the specified directory.\n"
    "\t\tWithout this options debugfs filesystem is assumed to be mounted\n"
    "\t\tinto '/sys/kernel/debug'.\n"
    "\t-s, --session\n"
    "\t\t Read only those messages from the trace, which collected from the\n"
    "\t\tmoment when target module is loaded to the moment when target module\n"
    "\t\tis unloaded. After the last message is read, stop capturing.\n"
    "\t-f, --file <file>\n"
    "\t\t Write all messages from the trace to the specified file.\n"
    "\t\tFilename '-' means STDOUT.\n"
    "\t-p, --program <program>\n"
    "\t\t Run the program specified, and write all messages from the trace\n"
    "\t\tto the program's STDIN.\n"
    "\n"
    "If neither '-f' nor '-p' option is specified, all messages from the trace\n"
    "will be written to STDOUT. Several options '-f' and/or '-p' are allowed.\n"
    "\n"
    "\t"PROGRAM_NAME" --help\n"
    "\n"
    " Print usage of this program and exit.\n"
    "\n";
    
/*
 * Callback function for filter lines in trace.
 * 
 * Return 1, if line should be processed,
 * return 0, if line should be skipped.
 * 
 * Set 'should_stop' to not 0 for stop trace processing.
 */
typedef int (*global_line_filter)(const char* str, size_t size,
    int *should_stop, void* data);

/*
 * Filter which start at the target loading and stop at the unloading.
 */
static int
filter_line_session(const char* str, size_t size,
    int* should_stop, void* data);

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
    /*
     * Global filter of trace lines.
     * 
     * NULL if no used.
     */
    global_line_filter filter;
    
    void* filter_data;
};

/*
 * Initialize list of trace_consumers as empty.
 */
static int
trace_consumers_init(struct trace_consumers* trace_consumers,
    global_line_filter filter, void* filter_data);

// Add consumer to the tail of the list.
static void trace_consumers_add_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer);

// Remove consumer from the list.
static void
trace_consumers_del_consumer(struct trace_consumers* trace_consumers,
    struct trace_consumer* consumer);

// Macro for iterate consumers in the list
#define trace_consumers_for_each(trace_consumers, consumer) \
    for((consumer) = (trace_consumers)->first_consumer; (consumer) != NULL; (consumer) = (consumer)->next)

/*
 * Free and remove all consumers from trace_consumers.
 * 
 * Waiting is not performed.
 */
static void trace_consumers_free(struct trace_consumers* trace_consumers);

/*
 * Free and remove all consumers from trace_consumers.
 * 
 * Also wait, while every child is finished.
 * 
 * Return 0, if every child exit with 0 status.
 * Otherwise return 1.
 */
static int trace_consumers_free_wait(struct trace_consumers* trace_consumers);


/*
 * Read one line from the trace file.
 * 
 * It is assumed, that:
 * 1) line is ended with '\n', and doesn't contain '\n' elsewhere
 * 2) one call to read() can read no more than one line from trace file
 * 3) reading from file may block only at the start of the line.
 * 
 * When line is read, its content is passed to the 'process_line' function.
 * 
 * On error, function return -1.
 * If trace is empty, return -2.(trace emptiness is effect of the STDIN signal)
 * 
 * Otherwise, result of 'process_line' function is returned.
 */

static int
read_trace_line(int fd,
    int (*process_line)(const char* str, size_t len, void* data),
    void* data);

/*
 * Calback for previous function.
 * 
 * Return 0, if line was processed.
 * Return 1, if reading trace should be stopped.
 * Return -1 on error(but with same meaning, as result 1).
 */
static int
trace_consumers_process_line(const char* str, size_t len, void* data);

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
 * Emulate next behaviour on SIGINT signal:
 * 
 * The first signal arrival emulates EOF of trace file, but do not
 * interrupt program.
 * 
 * Others signals work as usual.
 */

/*
 * Start block of code with behaviour explained above.
 * 
 * Return 0 on success.
 */

static int prepare_processing_sigint(void);

/*
 * End block of code with behaviour explained above.
 */

static void restore_processing_sigint(void);

/*
 * Test, whether signal SIGINT is arrived.
 * 
 * If so, return 1 and restore normal work of signal.
 * Otherwise return 0.
 * 
 * Note: in case function return 1, restore_processing_sigint() call
 * is needed anywhere.
 */

static int test_sigint(void);

/*
 * Return file descriptor, which may be used for watching signal using
 * select/poll mechanizms.
 * 
 * After test_sigint() return 1, this function returns -1.
 * 
 * Note: this function may not change signal disposition.
 */

static int get_signal_fd(void);

/*
 * Auxiliary function for polling the trace, 
 * in the same time watching for signal(if needed).
 * 
 * Return -1 on error.
 * Return 0 otherwise.
 * 
 * Restore signal disposition, if needed.
 */
static int poll_read(int fd);

/*
 * Auxiliary function for polling the pipe with other process, 
 * in the same time watching for signal(if needed).
 * 
 * Return -1 on error.
 * Return 0 otherwise.
 * 
 * Restore signal disposition, if needed.
 */
static int poll_write(int fd);

/*
 * Process options of the program and collect value of them
 * in the corresponded variables.
 * 
 * Return 0 on success.
 */
static int process_options(int argc, char* const argv[],
// output variables for collect options values
    //not 0 for 'session' mode
    int *is_session, 
    //
    const char** debugfs_mount_point,
    //NULL-terminated array of file names, should be freed after using.
    const char*** file_names,
    //NULL-terminated array of program names, should be freed after using.
    const char*** program_names
);

/*
 * Main.
 */

int main(int argc, char* const argv[], char* const envp[])
{
    int fd_trace;
    int result = 0;
    
    int is_session;
    const char* debugfs_mount_point;
    char const** file_names;
    char const** program_names;
    
    result = process_options(argc, argv,
        &is_session,
        &debugfs_mount_point,
        &file_names,
        &program_names);
    if(result)
        return result;

    
    size_t trace_file_name_len = snprintf(NULL, 0,
        "%s/%s", debugfs_mount_point, REL_TRACEFILE);
    char* trace_file_name = malloc(trace_file_name_len + 1);
    if(trace_file_name == NULL)
    {
        print_error0("Cannot allocate memory for trace file name.");
        free(program_names);
        free(file_names);
    }
    snprintf(trace_file_name, trace_file_name_len + 1,
        "%s/%s", debugfs_mount_point, REL_TRACEFILE);
    
    fd_trace = open(trace_file_name, O_RDONLY);
    if(fd_trace == -1)
    {
        switch(errno)
        {
        case ENOENT:
            print_error("Trace file '%s' is not exist.\n"
                "Probably, debugfs is not mounted into \"%s\".",
                trace_file_name, debugfs_mount_point);
        break;
        case EBUSY:
            print_error("Trace file is busy.\n"
                "Probably, another instance of %s process it.",
                PROGRAM_NAME);
        break;
        //case EACCESS: what message should be here?
        default:
            print_error("Cannot open trace file '%s' for read: %s.",
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
    trace_consumers_init(&trace_consumers,
        is_session ? filter_line_session : NULL, NULL);

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
    
    if(prepare_processing_sigint())
    {
        print_error0("Cannot prepare SIGINT processing.");
        trace_consumers_free(&trace_consumers);
        close(fd_trace);
        return -1;
    }
    
    do
    {
        int result = read_trace_line(fd_trace, trace_consumers_process_line,
                &trace_consumers);
        if(result == -1)
        {
            print_error0("Error occures while processing trace. Stop.");
            break;
        }
        else if((result == -2) || (result == 1))
        {
            //eof because of ending trace file or sigint or filtering trace.
            break;
        }
        //sleep(4);
        //printf("Next iteration of read.\n");
    }while(1);
    restore_processing_sigint();
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

static int poll_IO(int fd, short event)
{
    int result;
    struct pollfd pollfds[2] = {
        {
            .fd = -1,//will be fd
            .events = 0, //will be event
        },
        {
            .fd = -1,//will be fd_signals
            .events = POLLIN
        }
    };
    pollfds[0].fd = fd;
    pollfds[0].events = event;
    pollfds[1].fd = get_signal_fd();
    
    // Repeate poll() while interrupted with non-interesting signals
    do
    {
        result = poll(pollfds, (pollfds[1].fd != -1) ? 2 : 1, -1/*forever*/);
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
    return 0;
}

int poll_read(int fd) {return poll_IO(fd, POLLIN);}
int poll_write(int fd) {return poll_IO(fd, POLLOUT);}


struct trace_consumer*
trace_consumer_create_process(const char* command_line)
{
    struct trace_consumer* consumer;
    pid_t pid;
    int fd_pipe[2];
    //Allocate consumer struture
    consumer = malloc(sizeof(*consumer));
    if(consumer == NULL)
    {
        print_error0("Cannot allocate consumer struture.");
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
        print_error("Error when performs fork: %s.", strerror(errno));
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
 * Create descriptor, wia which one can write into the file
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
    //Allocate consumer struture
    consumer = malloc(sizeof(*consumer));
    if(consumer == NULL)
    {
        print_error0("Cannot allocate consumer struture.");
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
            print_error("Cannot open file \"%s\" for collect trace: %s.",
                filename, strerror(errno));
            trace_consumer_free(consumer);
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
            trace_consumer_free(consumer);
            return NULL;
        }

    }

    if(change_fl_flags(fd, O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for file for collect trace.");
        close(fd);
        trace_consumer_free(consumer);
        return NULL;
    }


    if(change_fd_flags(fd, FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for file for collect trace.");
        close(fd);
        trace_consumer_free(consumer);
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

int trace_consumers_init(struct trace_consumers* trace_consumers,
    global_line_filter filter, void* filter_data)
{
    trace_consumers->first_consumer = NULL;
    trace_consumers->last_consumer = NULL;
    
    trace_consumers->filter = filter;
    trace_consumers->filter_data = filter_data;
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
}

int trace_consumers_free_wait(struct trace_consumers* trace_consumers)
{
    int result = 0;
    struct trace_consumer* consumer;
    
    trace_consumers_for_each(trace_consumers, consumer)
    {
        trace_consumer_stop_write(consumer);
    }
    
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


static int fd_signal = -1;
static int is_signal_blocked = 0;

int get_signal_fd(void) {return is_signal_blocked ? fd_signal : -1;}

int prepare_processing_sigint(void)
{
    sigset_t signal_mask;
    
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    
    fd_signal = signalfd(-1, &signal_mask, 0);
    if(fd_signal == -1)
    {
        print_error("Cannot create file descriptor for signal SIGINT: %s.",
            strerror(errno));
        return -1;
    }
    if(change_fd_flags(fd_signal, FD_CLOEXEC, FD_CLOEXEC) == -1)
    {
        print_error0("Cannot set FD_CLOEXEC flag for signal file descriptor.");
        close(fd_signal);
        return -1;
    }
    if(change_fl_flags(fd_signal, O_NONBLOCK, O_NONBLOCK) == -1)
    {
        print_error0("Cannot set O_NONBLOCK flag for signal file descriptor.");
        close(fd_signal);
        return -1;
    }

    //Otherwise signal will not get into fd_signal...
    sigprocmask(SIG_BLOCK, &signal_mask, NULL);
    is_signal_blocked = 1;
    return 0;
}

static void restore_processing_sigint(void)
{
    if(is_signal_blocked)
    {
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGINT);
        
        sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
        is_signal_blocked = 0;
    }
    close(fd_signal);
}

int test_sigint(void)
{
    struct signalfd_siginfo siginfo;

    /*
     *  Consume signal in the signal file descriptor.
     *  Otherwise, it will 'alive', when be unblocked.
     */

    if(is_signal_blocked
        && read(fd_signal, &siginfo, sizeof(siginfo)) != -1)
    {
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGINT);
        sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
        is_signal_blocked = 0;
    }
    return !is_signal_blocked;
}

char* read_buffer = NULL;
int read_buffer_size = 0;

int
read_trace_line(int fd,
    int (*process_line)(const char* str, size_t len, void* data),
    void* data)
{
    int result;
    int size;//number of currently readed characters in the buffer.
    if(read_buffer == NULL)
    {
        read_buffer = malloc(READ_BUFFER_SIZE_MIN);
        if(read_buffer == NULL)
        {
            print_error0("Cannot allocate buffer for read from file.");
            errno = ENOMEM;
            return -1;
        }
        read_buffer_size = READ_BUFFER_SIZE_MIN;
    }
    if(test_sigint()) return -2;//eof by signal
    while(1)
    {
        size = read(fd, read_buffer, read_buffer_size);
        if(size != -1) break;
        switch(errno)
        {
        case EINTR:
        case ERESTART:
            break;
        case EAGAIN:
            if(poll_read(fd) == -1)
                return -1;
            if(test_sigint()) return -2;//eof by signal
            break;
        default:
            return -1;
        }
    }

    if(size == 0) return -2;//eof(strange, but may be correctly processed)
    
    while(read_buffer[size - 1] != '\n')
    {
        size_t size_tmp;
        size_t additional_size = read_buffer_size;
        read_buffer = realloc(read_buffer,
            read_buffer_size + additional_size);
        if(read_buffer == NULL)
        {
            print_error0("Cannot increase buffer for read from file.");
            return -1;
        }
        read_buffer_size = read_buffer_size + additional_size;
        
        while(1)
        {
            size_tmp = read(fd, read_buffer + size,
                additional_size);
            if(size_tmp > 0) break;
            if(size_tmp == 0)
            {
               print_error0("Line was partially read from file, "
                    "and next read encounter EOF.\n"
                    "Perhaps, there is another reader from file, "
                    "or file access is not aligned on lines.\n"
                    "Please, fix this.");
                return -1;
            }
            //Now 'size_tmp' is '-1'
            switch(errno)
            {
                case EINTR:
                case ERESTART:
                    break;
                case EAGAIN:
                    print_error0("Line was partially read from file, "
                        "and next read should block.\n"
                        "Perhaps, there is another reader from file, "
                        "or file access is not aligned on lines.\n"
                        "Please, fix this.");
                    return -1;
                default:
                    return -1;
            }
        }
        size += size_tmp;
    }
    result = process_line(read_buffer, size, data);
    if(read_buffer_size >= READ_BUFFER_SIZE_MAX)
    {
        free(read_buffer);
        read_buffer = NULL;
        read_buffer_size = 0;
    }
    return result;
}

/*
 * Same as write(), but repeat write until all bytes will be written
 * or real error occures.
 */
static ssize_t
write_str(int fd, const void* buffer, size_t size)
{
    size_t bytes_written = 0;
    do
    {
        ssize_t result = write(fd, buffer, size);
        if(result == -1)
        {
            if(errno == EINTR) continue;
            else if(errno == EAGAIN)
            {
                test_sigint();
                if(poll_write(fd) == -1) return -1;
                continue;
            }
            return -1;
        }
        bytes_written += result;
        buffer += result;
        size -= result;
    }
    while(size != 0);
    return bytes_written;
}


int
trace_consumers_process_line(const char* str, size_t len, void* data)
{
    int result = 0;
    int should_stop = 0;
    //Real type of 'data' parameter.
    struct trace_consumers *trace_consumers = (struct trace_consumers *)data;
    struct trace_consumer *consumer;
    
    if(trace_consumers->filter != NULL)
    {
        if(trace_consumers->filter(str, len, &should_stop,
            trace_consumers->filter_data) == 0)
        {
            goto out;
        }
        
    }

    trace_consumers_for_each(trace_consumers, consumer)
    {
        if(!trace_consumer_is_writeable(consumer)) continue;
        if(write_str(consumer->fd_write, str, len) == -1)
        {
            if((errno == EPIPE) && (consumer->pid != 0))
            {
                print_error0("Child process has closed its STDIN.");
            }
            else
            {
                print_error("Error occure while writting to the pipe with child process: %s.\n"
                    "Writing to this process will stop.",
                    strerror(errno));
            }
            trace_consumer_stop_write(consumer);
        }
    }
out:
    return should_stop ? 1 : 0;
}

int
filter_line_session(const char* str, size_t size,
    int* should_stop, void* data)
{

    static char start_marker[] = "target_session_begins:";
    static char end_marker[] = "target_session_ends:";
    static int markers_counter = 0;

    const char* marker;
    
    const char* str_end = str + size - 1;//do not process '\n'

#if (MESSAGE_COLUMN_NUMBER > 1)
    int column_number = 1;
    for(marker = str; marker < str_end; ++marker)
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
    marker = str;
#endif

#define is_substr(str, len, sub_str) \
    ((len >= strlen(sub_str)) && (strncmp(str, sub_str, strlen(sub_str)) == 0))
    
    if(is_substr(marker, str_end - marker, start_marker))
    {
        markers_counter++;
        return 1;
    }
    if(markers_counter == 0)
    {
        print_error("Trace line '%.*s' outside target session.",
            str_end - str, str);
        //only warning
        return 1;
    }
    if(is_substr(marker, str_end - marker, end_marker))
    {
        markers_counter--;
        if(markers_counter == 0) *should_stop = 1;
    }
#undef is_substr
    return 1;
}

// Available program's options
static const char short_options_h[] = "hsd:f:p:";
static struct option long_options_h[] = {
    {"help", 0, 0, 'h'},
    {"session", 0, 0, 's'},
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
    //not 0 for 'session' mode
    int *is_session, 
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
    *is_session = 0;
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
        print_error0("Cannot allocate memory for options(names of files).");
        return -1;
    }
    
    pnames = array_names_init();
    if(pnames == NULL)
    {
        print_error0("Cannot allocate memory for options(names of programs).");
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
                    "Call kedr_capture_trace -h for description of program's parameters.",
                    argv[optind]);
            free(pnames);
            free(fnames);
            return -1;
        case 's':
            *is_session = 1;
            break;
        case 'd':
            *debugfs_mount_point = optarg;
            break;
        case 'f':
            fnames = array_names_add(fnames, optarg);
            if(fnames == NULL)
            {
                print_error0("Cannot allocate memory for options(names of files).");
                free(pnames);
                return -1;
            }
            break;
        case 'p':
            pnames = array_names_add(pnames, optarg);
            if(pnames == NULL)
            {
                print_error0("Cannot allocate memory for options(names of programs).");
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
            print_error0("Cannot allocate memory for options(names of files).");
            free(pnames);
            return -1;
        }
    }
    
    *file_names = fnames;
    *program_names = pnames;
    
    return 0;
}
