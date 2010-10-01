#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>
    
#include <linux/mutex.h>

#include <linux/uaccess.h> /* copy_*_user functions */
    
#include <linux/debugfs.h>

#include <linux/ctype.h> /* isspace() */

#include <linux/string.h> /* memcpy */
    
MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/wobject/wobject.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

struct indicator_instance;

/*
 * Structure described simulation point
 */

struct kedr_simulation_point
{
    //for simulate
    struct indicator_instance* current_instance;
    //for searching point
    struct list_head list;
    const char* name;
    //for setting indicator
    const char* format_string;
    //control directory for work with point's indicators
    struct dentry* control_dir;
    struct dentry* format_string_file;
    struct file_operations format_string_fops;
    struct dentry* indicator_file;
    struct file_operations indicator_fops;
};

struct kedr_simulation_indicator
{
    //for searching generator
    struct list_head list;
    const char* name;
    //
    const char* format_string;
    //
    int (*simulate)(void* indicator_state, void* user_data);
    int (*create_instance)(void** indicator_state, const char* params, struct dentry* control_directory);
    void (*destroy_instance)(void* indicator_state);
    //
    wobj_t obj;
    //control directory for indicator
    struct dentry* control_dir;
};

struct indicator_instance
{
    struct kedr_simulation_indicator* indicator;
    void* indicator_state;
    //
    wobj_weak_ref_t indicator_weak_ref;
    //for destroy via callback of indicator_weak_ref
    struct kedr_simulation_point* current_point;
};
//List of points
static LIST_HEAD(points);
//Mutex which protect from concurrent addition/deletion of points and indicators and changing indicator instances of points
static DEFINE_MUTEX(points_mutex);
//List of indicators
static LIST_HEAD(indicators);

static struct dentry* root_directory;
static struct dentry* points_root_directory;
static struct dentry* indicators_root_directory;

// Auxiliary functions

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed under mutex taken.
 */

static struct kedr_simulation_point* lookup_point(const char* name);

/*
 * Same for indicators
 */

static struct kedr_simulation_indicator* lookup_indicator(const char* name);

/*
 * Clear indicator for given point.
 *
 * Should be executed under mutex taken.
 */

static void clear_indicator_internal(struct kedr_simulation_point* point);

/*
 *  Verify, whether data, which format is described in
 *  'point_format_string', will be correctly interpreted by indicator,
 *  which expect data in 'indicator_format_string' format.
 * 
 *  Return not 0 on success, 0 otherwise.
 */

static int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string);

/*
 * Callback which should clear instance of indicator(because indicator is need to delete).
 *
 * Note: it currently executed with mutex locked.
 */

static void clear_indicator_callback(wobj_weak_ref_t* instance_obj);

/*
 * For reuse function for exported kedr_fsim_point_set_indicator() and for
 * changing indicator via writting file
 *
 * Should be executed with mutex locked.
 */


int kedr_fsim_point_set_indicator_internal(struct kedr_simulation_point* point,
    const char* indicator_name, const char* params);

/*
 * Create directories and files for new point.
 */

static int create_point_files(struct kedr_simulation_point* point, struct module* m);
//
static void delete_point_files(struct kedr_simulation_point* point);

/*
 * Create directory for indicator
 */

static int create_indicator_files(struct kedr_simulation_indicator* indicator);
//
static void delete_indicator_files(struct kedr_simulation_indicator* indicator);

//////////////////Implementation of exported functions////////////////////

/*
 * Register simulation point with name 'point_name'.
 * 
 * Initially (before calling kedr_fsim_set_indicator)
 * point use no fault indicator, and
 * kedr_fsim_simulate for it return always 0.
 * 
 * Returning value may be used in kedr_fsim_simulate
 *  and kedr_fsim_point_unregister.
 * 
 * format_string should describe real format of user_data,
 * which may be passed to the kedr_fsim_simulate().
 * This format string is used to verify, whether particular indicator
 * fits for this simulation point.
 *
 * It is caller who responsible for passing user_data in correct format
 * to the kedr_fsim_simulate().
 *
 * If this name has already used for another point, returns NULL.
 */

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name,
	const char* format_string, struct module* m)
{
    struct kedr_simulation_point* point;
    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return NULL;
    }
    
    if(lookup_point(point_name))
    {
        mutex_unlock(&points_mutex);
        print_error("Point with name '%s' already registered.", point_name);
        return NULL;
    }
    point = kmalloc(sizeof(*point), GFP_KERNEL);
    if(point == NULL)
    {
        mutex_unlock(&points_mutex);
        print_error0("Cannot allocate memory for point.");
        return NULL;
    }
    point->name = point_name;
    point->format_string = format_string;
    point->current_instance = NULL;
    
    if(create_point_files(point, m))
    {
        mutex_unlock(&points_mutex);
        kfree(point);
        return NULL;
    }
    list_add(&point->list, &points);

    mutex_unlock(&points_mutex);
    
    return point;
}
EXPORT_SYMBOL(kedr_fsim_point_register);
/*
 * Unregister point, making its name free for use, and release resources.
 * 
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point)
{
    delete_point_files(point);//clear interface, without lock

    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return;
    }

    clear_indicator_internal(point);
    
    list_del(&point->list);

    kfree(point);

    mutex_unlock(&points_mutex);
}
EXPORT_SYMBOL(kedr_fsim_point_unregister);
/*
 * Register indicator.
 *
 * 'indicator_name' - name of the indicator created.
 *
 * 'simulate' - function which will be called at simulation stage
 *
 * 'format_string' - format of 'user_data' parameter, taken by 'simulate'
 *
 * 'create_instance' - function which will be called for create indicator instance
 * for particular point.
 *
 * 'destroy_instance' - function which will be called when indicator instance
 * should be unset for particular point
 *
 * Return not-null pointer, which may be used for unregister of indicator.
 *
 * If cannot create indicator for some reason(e.g., 'indicator_name' already used as name of other indicator),
 * return NULL.
 */

struct kedr_simulation_indicator* 
kedr_fsim_indicator_register(const char* indicator_name,
	int (*simulate)(void* indicator_state, void* user_data),
    const char* format_string,
    int (*create_instance)(void** indicator_state, const char* params, struct dentry* control_directory),
    void (*destroy_instance)(void* indicator_state)
)
{
    struct kedr_simulation_indicator *indicator;
    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return NULL;
    }

    if(lookup_indicator(indicator_name))
    {
        mutex_unlock(&points_mutex);
        print_error("Indicator with name '%s' already registered.", indicator_name);
        return NULL;
    }
    
    indicator = kmalloc(sizeof(*indicator), GFP_KERNEL);
    if(indicator == NULL)
    {
        mutex_unlock(&points_mutex);
        print_error0("Cannot allocate memory for indicator.");
        return NULL;
    }
    indicator->name = indicator_name;
    indicator->format_string = format_string;
    indicator->simulate = simulate;
    indicator->create_instance = create_instance;
    indicator->destroy_instance = destroy_instance;
    
    if(create_indicator_files(indicator))
    {
        mutex_unlock(&points_mutex);
        kfree(indicator);
        return NULL;
    }
    
    wobj_init(&indicator->obj, NULL);
    
    
    list_add(&indicator->list, &indicators);

    mutex_unlock(&points_mutex);
    
    return indicator;

}
EXPORT_SYMBOL(kedr_fsim_indicator_register);

/*
 * Unregister indicator, making its name free for use, and release resources.
 *
 * Also clear indicator for points, which are currently using insances of this indicator.
 */

void kedr_fsim_indicator_unregister(struct kedr_simulation_indicator* indicator)
{
    debug0("Start...");
    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return;
    }
    debug0("Deleting indicator...");
    list_del(&indicator->list);
    debug0("Before deleting files...");
    delete_indicator_files(indicator);
    debug0("wait until indicator instances become unused deleting files...");
    wobj_unref_final(&indicator->obj);
    kfree(indicator);

    mutex_unlock(&points_mutex);
    debug0("Finish...");
}
EXPORT_SYMBOL(kedr_fsim_indicator_unregister);

/*
 * Create and set instance of indicator with name 'indicator_name',
 * for point with name 'point_name'.
 *
 * Return 0 on success, not 0 - on fail.
 *
 * Note: there is a possibility, that kedr_simulation_point_simulate() will be called as with no indicator
 * while executing kedr_fsim_point_set_indicator().
 */

int kedr_fsim_point_set_indicator(const char* point_name,
    const char* indicator_name, const char* params)
{
    struct kedr_simulation_point* point;
    int result;
    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return -1;
    }
    
    point = lookup_point(point_name);
    if(point == NULL)
    {
        mutex_unlock(&points_mutex);
        print_error("Point with name '%s' isn't exist.", point_name);
        return -1;
    }

    result = kedr_fsim_point_set_indicator_internal(point, indicator_name, params);

    mutex_unlock(&points_mutex);
    
    return result;
}
EXPORT_SYMBOL(kedr_fsim_point_set_indicator);
/*
 * Clear and destroy indicator instance for given point, if it was set.
 */

int kedr_fsim_point_clear_indicator(const char* point_name)
{
    struct kedr_simulation_point* point;
    
    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return -1;
    }
    
    point = lookup_point(point_name);
    if(point == NULL)
    {
        mutex_unlock(&points_mutex);
        print_error("Point with name '%s' isn't exist.", point_name);
        return -1;
    }

    clear_indicator_internal(point);

    mutex_unlock(&points_mutex);
    
    return 0;
}
EXPORT_SYMBOL(kedr_fsim_point_clear_indicator);

/*
 * Call indicator, which was set for this point, and return result of indicator's
 * 'simulate' function.
 *
 * If indicator wasn't set for this point, return 0.
 */

int kedr_fsim_point_simulate(struct kedr_simulation_point* point,
    void *user_data)
{
    int result;
    struct indicator_instance* current_instance;
    rcu_read_lock();
    
    current_instance = rcu_dereference(point->current_instance);
    
    if(current_instance != NULL)
    {
        result = current_instance->indicator->simulate(current_instance->indicator_state, user_data);
    }
    else
    {
        result = 0;
    }
    rcu_read_unlock();
    return result;

}
EXPORT_SYMBOL(kedr_fsim_point_simulate);
///////////////////Implementation of auxiliary functions/////////////////////

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed under mutex taken.
 */

struct kedr_simulation_point* lookup_point(const char* name)
{
    struct kedr_simulation_point* point;
    list_for_each_entry(point, &points, list)
    {
        if(strcmp(point->name, name) == 0) return point;
    }
    return NULL;
}

/*
 * Same for indicators
 */

struct kedr_simulation_indicator* lookup_indicator(const char* name)
{
    struct kedr_simulation_indicator* indicator;
    list_for_each_entry(indicator, &indicators, list)
    {
        if(strcmp(indicator->name, name) == 0) return indicator;
    }
    return NULL;


}

/*
 * Clear indicator for given point.
 *
 * Should be executed under mutex taken.
 */

void clear_indicator_internal(struct kedr_simulation_point* point)
{
    struct indicator_instance* instance;
    wobj_t* indicator_obj;
    struct kedr_simulation_indicator* indicator;

    instance = point->current_instance;
    if(instance == NULL)
        return;

    indicator_obj = wobj_weak_ref_get(&instance->indicator_weak_ref);
    if(indicator_obj == NULL)
    {
        /*
         * Indicator is unref'ed under lock, so callback, which should cleared current indicator,
         * also should be executed under lock.
         *
         * Because we currently under same lock, we may be sure, that indicator object has real references now.
         * So, wobj_weak_ref_get() should always succeed.
         *
         */
        BUG();
    }
    
    rcu_assign_pointer(point->current_instance, NULL);
    synchronize_rcu();
    //now we can safetly remove indicator instance.
    indicator = container_of(indicator_obj, struct kedr_simulation_indicator, obj);
    indicator->destroy_instance(instance->indicator_state);
    //
    wobj_weak_ref_clear(&instance->indicator_weak_ref);
    wobj_unref(indicator_obj);
    //
    kfree(instance);
}

/*
 *  Verify, whether data, which format is described in
 *  'point_format_string', will be correctly interpreted by indicator,
 *  which expect data in 'indicator_format_string' format.
 * 
 *  Return not 0 on success, 0 otherwise.
 */

int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string)
{
	if(indicator_format_string == NULL
		|| *indicator_format_string == '\0')
	{
		//always compatible
		return 1;
	}
	else if(point_format_string == NULL
		|| *point_format_string == '\0')
	{
		//no data are passed, but indicator expects something
		return 0;
	}
	// simple verification, may be changed in the future
	return strncmp(point_format_string, indicator_format_string,
		strlen(indicator_format_string)) == 0;
}

/*
 * Callback which should clear instance of indicator(because indicator is need to delete).
 *
 * Note: it currently executed with motex locked.
 */

void clear_indicator_callback(wobj_weak_ref_t* instance_obj)
{
    struct indicator_instance* instance;
    struct kedr_simulation_point* point;
    struct kedr_simulation_indicator* indicator;
    
    instance = container_of(instance_obj, struct indicator_instance, indicator_weak_ref);
    point = instance->current_point;
    indicator = instance->indicator;
    BUG_ON(point == NULL);
    BUG_ON(indicator == NULL);
    BUG_ON(point->current_instance != instance);
    
    rcu_assign_pointer(point->current_instance, NULL);
    
    synchronize_rcu();
    
    indicator->destroy_instance(instance->indicator_state);
    kfree(instance);
}

/*
 * For reuse function for exported kedr_fsim_point_set_indicator() and for
 * changing indicator via writting file
 *
 * Should be executed with mutex locked.
 */

int kedr_fsim_point_set_indicator_internal(struct kedr_simulation_point* point,
    const char* indicator_name, const char* params)
{
    struct indicator_instance *instance;
    struct kedr_simulation_indicator* indicator;
    
    indicator = lookup_indicator(indicator_name);
    if(indicator == NULL)
    {
        print_error("Indicator with name '%s' isn't exist.", indicator_name);
        return -1;
    }

    if(!is_data_format_compatible(point->format_string, indicator->format_string))
    {
        print_error("Indicator with name '%s' has format of parameters '%s', "
            "which is not compatible with format '%s', using by point with name '%s'.",
            indicator_name, indicator->format_string,
            point->format_string, point->name);
        return -1;
    }
    
    instance = kmalloc(sizeof(*instance), GFP_KERNEL);
    if(instance == NULL)
    {
        print_error0("Cannot allocate memory for instance of indicator.");
        return -1;
    }

    clear_indicator_internal(point);
    //Because we take mutex, which protect indicator from deleting, we may safetly use this indicator
    if(indicator->create_instance)
    {
        if(indicator->create_instance(&instance->indicator_state, params, NULL/*should be fixed*/))
        {
            print_error("Fail to create instance of indicator '%s'.", indicator_name);
            kfree(instance);
            return -1;
        }
    }
    else
    {
        instance->indicator_state = NULL;
    }
    
    instance->indicator = indicator;
    wobj_weak_ref_init(&instance->indicator_weak_ref, &indicator->obj, clear_indicator_callback);

    instance->current_point = point;
    point->current_instance = instance;
    
    return 0;
}

///////////////////////////Files hierarchy///////////////////////

//helpers
//read string
static int string_operation_read(const char* str, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    size_t size = strlen(str) + 1;
    if(*f_pos < 0) return 0;
    if(*f_pos >= size) return 0;
    if(count + *f_pos > size) count = size - *f_pos;
    if(copy_to_user(buf, str + *f_pos, count) != 0)
        return -EFAULT;
    
    *f_pos += count;
    return count;
}
//'implement' size of string via llseek
loff_t string_operation_llseek (const char* str, loff_t *f_pos, loff_t offset, int whence)
{
    loff_t new_offset;
    size_t size = strlen(str) + 1;
    switch(whence)
    {
    case 0: /* SEEK_SET */
        new_offset = offset;
    break;
    case 1: /* SEEK_CUR */
        new_offset = *f_pos + offset;
    break;
    case 2: /* SEEK_END */
        new_offset = size + offset;
    break;
    default: /* can't happen */
        return -EINVAL;
    };
    if(new_offset < 0) return -EINVAL;
    if(new_offset >= size) new_offset = size;//eof
    
    *f_pos = new_offset;
    return new_offset;//returning value is offset from the beginning, filp->fpos, generally speaking, may be any
}

static int
point_indicator_file_open (struct inode *inode, struct file *filp)
{
    if(O_TRUNC)//same as write string with only spaces
    {
        //clear indicator
        struct kedr_simulation_point* point = (struct kedr_simulation_point*)inode->i_private;
        if(mutex_lock_killable(&points_mutex))
        {
            debug0("Was killed");
            return -EINTR;
        }
        clear_indicator_internal(point);
        mutex_unlock(&points_mutex);
    }

    filp->private_data = inode->i_private;
    return 0;
}

static loff_t
point_indicator_file_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t result;

    struct indicator_instance* instance;
    const char* str;
    
    struct kedr_simulation_point* point = filp->private_data;
    BUG_ON(point == NULL);

    instance = rcu_dereference(point->current_instance);
    str = instance ? instance->indicator->name : "";
    
    //if(!mutex_trylock(&points_mutex)) return -EINTR;//preventing deadlock
    rcu_read_lock();
    
    result = string_operation_llseek(str, &filp->f_pos, off, whence);
    
    rcu_read_unlock();
    return result;
}

static ssize_t 
point_indicator_file_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    ssize_t result;
    struct indicator_instance* instance;
    const char* str;
    
    struct kedr_simulation_point* point = filp->private_data;
    BUG_ON(point == NULL);
    
    //if(!mutex_trylock(&points_mutex)) return -EINTR;//preventing deadlock
    rcu_read_lock();
    
    instance = rcu_dereference(point->current_instance);
    str = instance ? instance->indicator->name : "";
    
    result = string_operation_read(str, buf, count, f_pos);
    
    rcu_read_unlock();
    return result;
}
//real control function for point
static ssize_t 
point_indicator_file_write(struct file *filp, const char __user *buf,
    size_t count, loff_t *f_pos)
{
    struct kedr_simulation_point* point;
    char* buffer;
    const char* indicator_name;
    const char* params;

    ssize_t result;
    
    point = filp->private_data;
    BUG_ON(point == NULL);

    if((count == 0) || (*f_pos != 0))
    {
        debug0("Incorrect count or fpos");
        return -EINVAL;
    }
    
    buffer = kmalloc(count + 1, GFP_KERNEL);
    if(buffer == NULL)
    {
        print_error0("Cannot allocate buffer.");
        return -ENOMEM;
    }
    if(copy_from_user(buffer, buf, count) != 0)
    {
        debug0("copy_from_user return error.");
        kfree(buffer);
        return -EFAULT;
    }
    // For case, when one try to write not null-terminated sequence of bytes,
    // or omit terminated null-character.
    buffer[count] = '\0';
    debug("Writting string is '%s'", buffer);
    //parce buffer
    indicator_name = buffer;
    //trim leading spaces from indicator name
    while(isspace(*indicator_name)) indicator_name++;
    //look for beginning of params
    for(params = indicator_name; *params != '\0'; params++)
    {
        if(isspace(*params))
        {
            //separate indicator_name from parameters
            buffer[params - buffer]= '\0';
            //trim leading spaces in params
            params++;
            while(isspace(*params)) params++;
            break;
        }
    }

    if(mutex_lock_killable(&points_mutex))
    {
        debug0("Was killed");
        return -1;
    }
    debug0("Before setting of indicator");
    if(*indicator_name == '\0')
    {
        clear_indicator_internal(point);
        result = 0;
    }
    else
        result = kedr_fsim_point_set_indicator_internal(point, indicator_name, params);
    debug0("After setting of indicator");
    mutex_unlock(&points_mutex);

    kfree(buffer);
    return result ? -EINVAL : count;
}

static struct file_operations point_indicator_file_operations =
{
    .owner = THIS_MODULE,//
    .llseek = point_indicator_file_llseek, //size
    .read = point_indicator_file_read, //get current indicator
    .write = point_indicator_file_write, //set current indicator
    .open = point_indicator_file_open, // copy pointer to point from inode into filp
};


static int
point_format_string_file_open (struct inode *inode, struct file *filp)
{
    filp->private_data = inode->i_private;
    return 0;
}

static loff_t
point_format_string_file_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t result;
    
    struct kedr_simulation_point* point = filp->private_data;
    BUG_ON(point == NULL);

    result = string_operation_llseek(point->format_string ?
        point->format_string : "", &filp->f_pos, off, whence);
    
    return result;
}

static ssize_t 
point_format_string_file_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    ssize_t result;
    
    struct kedr_simulation_point* point = filp->private_data;
    BUG_ON(point == NULL);
    
    result = string_operation_read(point->format_string ?
        point->current_instance->indicator->name : "", buf, count, f_pos);
    
    return result;
}


static struct file_operations point_format_string_file_operations =
{
    .owner = THIS_MODULE,//
    .llseek = point_format_string_file_llseek, //size
    .read = point_format_string_file_read, //read format string
    .open = point_format_string_file_open, // copy pointer to point from inode into filp
};

/*
 * Create directories and files for new point.
 */

static int create_point_files(struct kedr_simulation_point* point, struct module* m)
{
    point->control_dir = debugfs_create_dir(point->name, points_root_directory);
    if(point->control_dir == NULL)
    {
        print_error0("Cannot create control directory for the point.");
        return -1;
    }
    
    memcpy(&point->indicator_fops, &point_indicator_file_operations,
        sizeof(point->indicator_fops));
    point->indicator_fops.owner = m;
    point->indicator_file = debugfs_create_file("current_indicator", 
        S_IRUGO | S_IWUSR | S_IWGRP,
        point->control_dir,
        point, &point->indicator_fops);
    if(point->indicator_file == NULL)
    {
        print_error0("Cannot create indicator file for the point.");
        debugfs_remove(point->control_dir);
        point->control_dir = NULL;
        return -1;
    }
    
    memcpy(&point->format_string_fops, &point_format_string_file_operations,
        sizeof(point->format_string_fops));
    point->format_string_fops.owner = m;
    point->format_string_file = debugfs_create_file("format_string", 
        S_IRUGO,
        point->control_dir,
        point, &point->format_string_fops);
    if(point->format_string_file == NULL)
    {
        print_error0("Cannot create format string file for the point.");
        debugfs_remove(point->indicator_file);
        point->indicator_file = NULL;
        debugfs_remove(point->control_dir);
        point->control_dir = NULL;
        return -1;
    }
    return 0;
}
//
static void delete_point_files(struct kedr_simulation_point* point)
{
    if(point->format_string_file)
        debugfs_remove(point->format_string_file);
    if(point->indicator_file)
        debugfs_remove(point->indicator_file);
    if(point->control_dir)
        debugfs_remove(point->control_dir);
}

/*
 * Create directory for indicator
 */

static int create_indicator_files(struct kedr_simulation_indicator* indicator)
{
    indicator->control_dir = debugfs_create_dir(indicator->name, indicators_root_directory);
    if(indicator->control_dir == NULL)
    {
        print_error0("Cannot create control directory for the indicator.");
        return -1;
    }
    return 0;
}
//
static void delete_indicator_files(struct kedr_simulation_indicator* indicator)
{
    debugfs_remove(indicator->control_dir);
}

/////////////////////////////////////////////////////////////////////////////

static int __init
kedr_fault_simulation_init(void)
{
    root_directory = debugfs_create_dir("kedr_fault_simulation", NULL);
    if(root_directory == NULL)
    {
        print_error0("Cannot create root directory in debugfs for service.");
        return -1;
    }
    points_root_directory = debugfs_create_dir("points", root_directory);
    if(points_root_directory == NULL)
    {
        print_error0("Cannot create directory in debugfs for points.");
        debugfs_remove(root_directory);
        return -1;
    }
    indicators_root_directory = debugfs_create_dir("indicators", root_directory);
    if(indicators_root_directory == NULL)
    {
        print_error0("Cannot create directory in debugfs for indicators.");
        debugfs_remove(points_root_directory);
        debugfs_remove(root_directory);
        return -1;
    }
    return 0;
}

static void
kedr_fault_simulation_exit(void)
{
    BUG_ON(!list_empty(&points));
    BUG_ON(!list_empty(&indicators));

    debugfs_remove(points_root_directory);
    debugfs_remove(indicators_root_directory);
    debugfs_remove(root_directory);
}
module_init(kedr_fault_simulation_init);
module_exit(kedr_fault_simulation_exit);