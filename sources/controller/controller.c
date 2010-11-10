/*
 * The "controller" component of KEDR system. 
 * Its main responsibility is to instrument the target module
 * to allow call interception.
 *
 * Copyright (C) 2010 Institute for System Programming 
 *                    of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <asm/insn.h>       /* instruction decoder machinery */

#include <kedr/base/common.h>   /* common declarations */

/* To minimize the unexpected consequences of trace event-related 
 * headers and symbols, place #include directives for system headers 
 * before '#define CREATE_TRACE_POINTS' directive
 */
#define CREATE_TRACE_POINTS
#include "controller_tracing.h" /* trace events */

/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "controller: "

/* ================================================================ */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/* ========== Module Parameters ========== */
/* 
 * Name of the module to analyze. It can be passed to 'insmod' as 
 * an argument, for example,
 *  /sbin/insmod cp_controller.ko target_name="module_to_be_analyzed"
 */
static char* target_name = ""; /* an empty name will match no module */
module_param(target_name, charp, S_IRUGO);

// to be implemented in some later version:
/*
 * If 0, the controller will not instrument the target module if the latter
 * is already loaded, that is, if the target has been loaded earlier than
 * the controller. The controller will issue a warning and fail to load in
 * this case.
 * 
 * If this parameter is nonzero, the controller will instrument the target
 * module even if the module is already loaded.
 */
//int instrument_if_loaded = 0;
//module_param(instrument_if_loaded, int, S_IRUGO);

/* ================================================================ */
/* A mutex to protect most of the global data in the controler - except 
 * those protected by the special spinlocks, see below.
 */
DEFINE_MUTEX(controller_mutex);

/* The module being analyzed. NULL if the module is not currently loaded. */
struct module* target_module = NULL;

/* Nonzero if the target module is initializing, 0 if it has completed 
 * initialization or if no target is loaded at the moment. */
int target_in_init = 0;

/* A spinlock to protect target_in_init from concurrent access. 
 * A mutex would not do because target_in_init can be accessed from atomic
 * context too (kedr_target_module_in_init() can be called from a replacement
 * function executing in atomic context).
 */
DEFINE_SPINLOCK(target_in_init_lock);

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not.
 */
int handle_module_notifications = 0;
/* ================================================================ */

/* The replacement table (the data pointed-to by its fields is owned by 
 * kedr-base) 
 */
struct kedr_repl_table repl_table;

/* ================================================================ */
/* Helpers */

/* CALL_ADDR_FROM_OFFSET()
 * 
 * Calculate the memory address being the operand of a given instruction 
 * (usually, 'call'). 
 *   'insn_addr' is the address of the instruction itself,
 *   'insn_len' is length of the instruction in bytes,
 *   'offset' is the offset of the destination address from the first byte
 *   past the instruction.
 * 
 * For x86_64 architecture, the offset value is sign-extended here first.
 * 
 * "Intel x86 Instruction Set Reference" states the following 
 * concerning 'call rel32':
 * 
 * "Call near, relative, displacement relative to next instruction.
 * 32-bit displacement sign extended to 64 bits in 64-bit mode."
 * *****************************************************************
 * 
 * CALL_OFFSET_FROM_ADDR()
 * 
 * The reverse of CALL_ADDR_FROM_OFFSET: calculates the offset value
 * to be used in 'call' instruction given the address and length of the
 * instruction and the address of the destination function.
 * 
 * */
#ifdef CONFIG_X86_64
#  define CALL_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
    (void*)((s64)(insn_addr) + (s64)(insn_len) + (s64)(s32)(offset))

#else /* CONFIG_X86_32 */
#  define CALL_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
    (void*)((u32)(insn_addr) + (u32)(insn_len) + (u32)(offset))
#endif

#define CALL_OFFSET_FROM_ADDR(insn_addr, insn_len, dest_addr) \
    (u32)(dest_addr - (insn_addr + (u32)insn_len))
/* ================================================================ */

/* ================================================================ */
/* Decode and process the instruction ('c_insn') at
 * the address 'kaddr' - see the description of do_process_area for details. 
 * 
 * Check if we get past the end of the buffer [kaddr, end_kaddr)
 * 
 * The function returns the length of the instruction in bytes. 
 * 0 is returned in case of failure.
 */
static unsigned int
do_process_insn(struct insn* c_insn, void* kaddr, void* end_kaddr,
    void** from_funcs, void** to_funcs, unsigned int nfuncs)
{
    /* ptr to the 32-bit offset argument in the instruction */
    u32* offset = NULL; 
    
    /* address of the function being called */
    void* addr = NULL;
    
    static const unsigned char op_call = 0xe8; /* 'call <offset>' */
    static const unsigned char op_jmp  = 0xe9; /* 'jmp  <offset>' */
    
    int i;
    
    BUG_ON(from_funcs == NULL || to_funcs == NULL);
    
    /* Decode the instruction and populate 'insn' structure */
    kernel_insn_init(c_insn, kaddr);
    insn_get_length(c_insn);
    
    if (c_insn->length == 0)
    {
        return 0;
    }
    
    if (kaddr + c_insn->length > end_kaddr)
    {
    /* Note: it is OK to stop at 'end_kaddr' but no further */
        KEDR_MSG(COMPONENT_STRING
    "instruction decoder stopped past the end of the section.\n");
        insn_get_opcode(c_insn);
        printk(KERN_ALERT COMPONENT_STRING 
    "kaddr=%p, end_kaddr=%p, c_insn->length=%d, opcode=0x%x\n",
            (void*)kaddr,
            (void*)end_kaddr,
            (int)c_insn->length,
            (unsigned int)c_insn->opcode.value
        );
        WARN_ON(1);
    }
        
/* This call may be overkill as insn_get_length() probably has to decode 
 * the instruction completely.
 * Still, to operate safely, we need insn_get_opcode() before we can access
 * c_insn->opcode. 
 * The call is cheap anyway, no re-decoding is performed.
 */
    insn_get_opcode(c_insn); 
    if (c_insn->opcode.value != op_call &&
        c_insn->opcode.value != op_jmp)
    {
        /* Neither 'call' nor 'jmp' instruction, nothing to do. */
        return c_insn->length;
    }
    
/* [NB] For some reason, the decoder stores the argument of 'call' and 'jmp'
 * as 'immediate' rather than 'displacement' (as Intel manuals name it).
 * May be it is a bug, may be it is not. 
 * Meanwhile, I'll call this value 'offset' to avoid confusion.
 */

    /* Call this before trying to access c_insn->immediate */
    insn_get_immediate(c_insn);
    if (c_insn->immediate.nbytes != 4)
    {
        KEDR_MSG(COMPONENT_STRING 
    "at 0x%p: "
    "opcode: 0x%x, "
    "immediate field is %u rather than 32 bits in size; "
    "insn.length = %u, insn.imm = %u, off_immed = %d\n",
            kaddr,
            (unsigned int)c_insn->opcode.value,
            8 * (unsigned int)c_insn->immediate.nbytes,
            c_insn->length,
            (unsigned int)c_insn->immediate.value,
            insn_offset_immediate(c_insn));
        WARN_ON(1);
        return c_insn->length;
    }
    
    offset = (u32*)(kaddr + insn_offset_immediate(c_insn));
    addr = CALL_ADDR_FROM_OFFSET(kaddr, c_insn->length, *offset);
    
    /* Check if one of the functions of interest is called */
    for (i = 0; i < nfuncs; ++i)
    {
        if (addr == from_funcs[i])
        {
        /* Change the address of the function to be called */
            BUG_ON(to_funcs[i] == NULL);
            
            KEDR_MSG(COMPONENT_STRING 
    "at 0x%p: changing address 0x%p to 0x%p (displ: 0x%x to 0x%x)\n",
                kaddr,
                from_funcs[i], 
                to_funcs[i],
                (unsigned int)(*offset),
                (unsigned int)CALL_OFFSET_FROM_ADDR(
                    kaddr, c_insn->length, to_funcs[i])
            );
            
            *offset = CALL_OFFSET_FROM_ADDR(
                kaddr, 
                c_insn->length,
                to_funcs[i]
            );
            
            break;
        }
    }
    
    return c_insn->length;
}

/* Process the instructions in [kbeg, kend) area.
 * Each 'call' instruction calling one of the target functions will be 
 * changed so as to call the corresponding replacement function instead.
 * The addresses of target and replacement fucntions are given in
 * 'from_funcs' and 'to_funcs', respectively, the number of the elements
 * to process in these arrays being 'nfuncs'.
 * For each i=0..nfuncs-1, from_funcs[i] corresponds to to_funcs[i].
 */
static void
do_process_area(void* kbeg, void* kend, 
    void** from_funcs, void** to_funcs, unsigned int nfuncs)
{
    struct insn c_insn; /* current instruction */
    void* pos = NULL;
    
    BUG_ON(kbeg == NULL);
    BUG_ON(kend == NULL);
    BUG_ON(kend < kbeg);
        
    for (pos = kbeg; pos + 4 < kend; )
    {
        unsigned int len;
        unsigned int k;

/* 'pos + 4 < kend' is based on another "heuristics". 'call' and 'jmp' 
 * instructions we need to instrument are 5 bytes long on x86 and x86-64 
 * machines. So if there are no more than 4 bytes left before the end, they
 * cannot contain the instruction of this kind, we do not need to check 
 * these bytes. 
 * This allows to avoid "decoder stopped past the end of the section"
 * conditions (see do_process_insn()). There, the decoder tries to chew 
 * the trailing 1-2 zero bytes of the section (padding) and gets past 
 * the end of the section.
 * It seems that the length of the instruction that consists of zeroes
 * only is 3 bytes (it is a flavour of 'add'), i.e. shorter than that 
 * kind of 'call' we are instrumenting.
 *
 * [NB] The above check automatically handles 'pos == kend' case.
 */
       
        len = do_process_insn(&c_insn, pos, kend,
            from_funcs, to_funcs, nfuncs);
        if (len == 0)   
        {
            KEDR_MSG(COMPONENT_STRING
                "do_process_insn() returned 0\n");
            WARN_ON(1);
            break;
        }

        if (pos + len > kend)
        {
            break;
        }
        
/* If the decoded instruction contains only zero bytes (this is the case,
 * for example, for one flavour of 'add'), skip to the first nonzero byte
 * after it. 
 * This is to avoid problems if there are two or more sections in the area
 * being analysed. Such situation is very unlikely - still have to find 
 * the example. Note that ctors and dtors seem to be placed to the same 
 * '.text' section as the ordinary functions ('.ctors' and '.dtors' sections
 * probably contain just the lists of their addresses or something similar).
 * 
 * As we are not interested in instrumenting 'add' or the like, we can skip 
 * to the next instruction that does not begin with 0 byte. If we are 
 * actually past the last instruction in the section, we get to the next 
 * section or to the end of the area this way which is what we want in this
 * case.
 */
        for (k = 0; k < len; ++k)
        {
            if (*((unsigned char*)pos + k) != 0) 
            {
                break;
            }
        }
        pos += len;
        
        if (k == len) 
        {
            /* all bytes are zero, skip the following 0s */
            while (pos < kend && *(unsigned char*)pos == 0)
            {
                ++pos;
            }
        }
    }
    
    return;
}

/* Replace all calls to to the target functions with calls to the 
 * replacement-functions in the module. 
 */
static void 
replace_calls_in_module(struct module* mod)
{
    BUG_ON(mod == NULL);
    BUG_ON(mod->module_core == NULL);

    if (mod->module_init != NULL)
    {
        KEDR_MSG(COMPONENT_STRING 
            "target module: \"%s\", processing \"init\" area\n",
            module_name(mod));
            
        do_process_area(mod->module_init, 
            mod->module_init + mod->init_text_size,
            repl_table.orig_addrs,
            repl_table.repl_addrs,
            repl_table.num_addrs);
    }

    KEDR_MSG(COMPONENT_STRING 
        "target module: \"%s\", processing \"core\" area\n",
        module_name(mod));
        
    do_process_area(mod->module_core, 
        mod->module_core + mod->core_text_size,
        repl_table.orig_addrs,
        repl_table.repl_addrs,
        repl_table.num_addrs);
    return;
}

/* ================================================================== */
/* Module filter.
 * Should return nonzero if detector should watch for module with this name.
 */
static int 
filter_module(const char *mod_name)
{
    /* We are interested only in analysing the module which name
     * is specified in 'target' parameter. 
     * */
    return strcmp(mod_name, target_name) == 0;
}

/*
 * on_module_load() should do real work when the target module is loaded:
 * instrument it, etc.
 *
 * Note that this function is called with controller_mutex locked.
 */
static void 
on_module_load(struct module *mod)
{
    int ret = 0;
    unsigned long flags;
    
    KEDR_MSG(COMPONENT_STRING 
        "target module \"%s\" has just loaded.\n",
        module_name(mod));
    
    spin_lock_irqsave(&target_in_init_lock, flags);
    target_in_init = 1;
    spin_unlock_irqrestore(&target_in_init_lock, flags);
    
    trace_target_session_begins(target_name);
    /* Until this function finishes, no replacement function will be called
     * because the target module has not completed loading yet. That means,
     * no tracepoint will be triggered in the target module before the 
     * tracepoint above is triggered. The order of the messages in the trace
     * is still up to the tracing system.
     */
    
    /* Notify the base and request the combined replacement table */
    ret = kedr_impl_on_target_load(target_module, &repl_table);
    if (ret != 0)
    {
        KEDR_MSG(COMPONENT_STRING
        "failed to handle loading of the target module.\n");
        return;
    }
    
    replace_calls_in_module(mod);
    return;
}

/*
 * on_module_unload() should do real work when the target module is about to
 * be unloaded. 
 *
 * Note that this function is called with controller_mutex locked.
 *
 * [NB] This function is called even if initialization of the target module 
 * fails.
 * */
static void 
on_module_unload(struct module *mod)
{
    int ret = 0;
    unsigned long flags;
    
    KEDR_MSG(COMPONENT_STRING 
        "target module \"%s\" is going to unload.\n",
        module_name(mod));
    
/* The replacement table may be used no longer. 
 * The base will take care of releasing its contents when appropriate.
 * [NB] The access to repl_table is already synchronized as on_module_unload()
 * is called with controller_mutex locked.
 */
    repl_table.num_addrs = 0;
    repl_table.orig_addrs = NULL;
    repl_table.repl_addrs = NULL;    
    
    spin_lock_irqsave(&target_in_init_lock, flags);
    target_in_init = 0; 
    spin_unlock_irqrestore(&target_in_init_lock, flags);
    
    /* Notify the base */
    ret = kedr_impl_on_target_unload(target_module);
    if (ret != 0)
    {
        KEDR_MSG(COMPONENT_STRING
        "failed to handle unloading of the target module.\n");
        return;
    }
    
    trace_target_session_ends(target_name);
    return;
}

/* A callback function to catch loading and unloading of module. 
 * Sets target_module pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
    unsigned long mod_state, void *vmod)
{
    struct module* mod = (struct module *)vmod;
    BUG_ON(mod == NULL);
    
    if (mutex_lock_interruptible(&controller_mutex) != 0)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock controller_mutex\n");
        return 0;
    }
    
    if (handle_module_notifications)
    {
        /* handle module state change */
        switch(mod_state)
        {
        case MODULE_STATE_COMING: /* the module has just loaded */
            if(!filter_module(module_name(mod))) break;
            
            BUG_ON(target_module != NULL);
            target_module = mod;
            on_module_load(mod);
            break;
        
        case MODULE_STATE_GOING: /* the module is going to unload */
        /* if the target module has already been unloaded, 
         * target_module is NULL, so (mod != target_module) will
         * be true. */
            if(mod != target_module) break;
            
            on_module_unload(mod);
            target_module = NULL;
        }
    }

    mutex_unlock(&controller_mutex);
    return 0;
}

/* ================================================================ */
/* A struct for watching for loading/unloading of modules.*/
struct notifier_block detector_nb = {
    .notifier_call = detector_notifier_call,
    .next = NULL,
    .priority = 3, /*Some number*/
};
/* ================================================================ */

/* Returns nonzero if a target module is currently loaded and it executes 
 * its init function at the moment, 0 otherwise (0 is returned even if there
 * is no target module loaded at the moment).
 * See the description of kedr_target_module_in_init().
 *
 * Actually, kedr_target_module_in_init() delegates its work to this 
 * function. In addition, kedr-base must ensure that no concurrent execution
 * of this delegate function occurs.
 */
static int
is_target_module_in_init(void)
{
/*
 * The target cannot unload before this function exits.
 * Indeed, is_target_module_in_init() is called by kedr_target_module_in_init(),
 * which may only be called from the replacement functions. The replacement 
 * functions are called from the code of the target driver. As long as at
 * least one replacement function is working, the target module is still 
 * there because it is in use by some task.
 */
    unsigned long flags;
    int result;
    
    spin_lock_irqsave(&target_in_init_lock, flags);
    if (target_in_init)
    {
        target_in_init = (target_module->module_init != NULL);
    }
    result = target_in_init;
    spin_unlock_irqrestore(&target_in_init_lock, flags);
    
    /* [NB] When the target is unloaded, on_module_unload() will set 
     * target_in_init to 0.*/
    return result;
}

/* The structure representing this controller */
struct kedr_impl_controller controller = {
    .mod = THIS_MODULE,
    .delegates.target_module_in_init = is_target_module_in_init,
};  

/* ================================================================ */
static void
controller_cleanup_module(void)
{
    unregister_module_notifier(&detector_nb);
    kedr_impl_controller_unregister(&controller);
    
/* TODO later: uninstrument target if it is still loaded.
 * This makes sense only if there is a reasonable safe way of instrumenting
 * a live module ("hot patching") available.
 */
    KEDR_MSG(COMPONENT_STRING 
        "cleanup successful\n");
    return;
}

/* ================================================================ */
static int __init
controller_init_module(void)
{
    int result = 0;
    KEDR_MSG(COMPONENT_STRING
        "initializing\n");
    
    /* Register with the base - must do this before the controller 
     * begins to respond to module load/unload notifications.
     */
    result = kedr_impl_controller_register(&controller);
    if (result < 0)
    {
        goto fail;
    }
    
    /* When looking for the target module, module_mutex must be locked */
    result = mutex_lock_interruptible(&module_mutex);
    if (result != 0)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock module_mutex\n");
        goto fail;
    }
    
    result = register_module_notifier(&detector_nb);
    if (result < 0)
    {
        goto unlock_and_fail;
    }
    
    /* Check if the target is already loaded */
    if (find_module(target_name) != NULL)
    {
        KEDR_MSG(COMPONENT_STRING
            "target module \"%s\" is already loaded\n",
            target_name);
        
        KEDR_MSG(COMPONENT_STRING
    "instrumenting already loaded target modules is not supported\n");
        result = -EEXIST;
        goto unlock_and_fail;
    }
    
    result = mutex_lock_interruptible(&controller_mutex);
    if (result != 0)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock controller_mutex\n");
        goto fail;
    }
    handle_module_notifications = 1;
    mutex_unlock(&controller_mutex);
    
    mutex_unlock(&module_mutex);
        
/* From now on, the controller will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload
 * from memory.
 */
    return 0; /* success */

unlock_and_fail:
    mutex_unlock(&module_mutex);
fail:
    controller_cleanup_module();
    return result;
}

static void __exit
controller_exit_module(void)
{
    controller_cleanup_module();
    return;
}

module_init(controller_init_module);
module_exit(controller_exit_module);
/* ================================================================ */
