/*
 * The "controller" component of KEDR system. 
 * Its main responsibility is to instrument the target module
 * to allow call interception.
 *
 * Copyright (C) 2010 Institute for System Programming 
 *		              of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *		Eugene A. Shatokhin <spectre@ispras.ru>
 *		Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
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

	
/* [NB] For now, we just don't care of some synchronization issues.
 * */

/* TODO: protect access to target_module with a mutex.
 * 
 * */
 
/* TODO: revisit locking modules in memory */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/errno.h>	/* error codes */
#include <linux/list.h>		/* linked lists */

#include <asm/insn.h>		/* instruction decoder machinery */

#include <kedr/base/common.h>	/* common declarations */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/* ========== Module Parameters ========== */
/* Name of the module to analyse. It can be passed to 'insmod' as 
 * an argument, for example,
 * 	/sbin/insmod cp_controller.ko target_name="module_to_be_analysed"
 * */
static char* target_name = ""; /* an empty name will match no module */
module_param(target_name, charp, S_IRUGO);
/* ================================================================ */

/* The module being analysed. */
struct module* target_module = NULL;

/* Nonzero if the target module is initializing, 0 if it has completed 
 * initialization or if no target is loaded at the moment. */
int target_in_init = 0;

/* ================================================================ */
struct payload_module_list
{
	struct list_head list;
	struct kedr_payload* payload;
};

/* The list of currently loaded payload modules - 
 * effectively, the list of 'struct kedr_payload*' */
struct list_head payload_modules;
/* ================================================================ */

/* The combined replacement table  */
void** orig_addrs = NULL;
void** repl_addrs = NULL;
unsigned int num_addrs = 0;

/* ================================================================ */
/* Free the combined replacement table, reset the pointers to NULL and 
 * the number of elements - to 0. */
static void
free_repl_table(void*** ptarget_funcs, void*** prepl_funcs, 
	unsigned int* pnum_funcs)
{
	BUG_ON(ptarget_funcs == NULL);
	BUG_ON(prepl_funcs == NULL);
	BUG_ON(pnum_funcs == NULL);
	
	kfree(*ptarget_funcs);
	kfree(*prepl_funcs);
	
	*ptarget_funcs = NULL;
	*prepl_funcs = NULL;
	*pnum_funcs = 0;
	return;
}

/* Allocate appropriate amount of memory and combine the replacement 
 * tables from all payload modules into a single 'table'. Actually, 
 * the table is returned in two arrays, '*ptarget_funcs' and '*repl_funcs',
 * the number of elements in each one is returned in '*pnum_funcs'.
 * */
static int
create_repl_table(void*** ptarget_funcs, void*** prepl_funcs, 
	unsigned int* pnum_funcs)
{
	struct payload_module_list* entry;
	struct list_head *pos;
	unsigned int i;
	
	BUG_ON(ptarget_funcs == NULL);
	BUG_ON(prepl_funcs == NULL);
	BUG_ON(pnum_funcs == NULL);
	
	*ptarget_funcs = NULL;
	*prepl_funcs = NULL;
	*pnum_funcs = 0;
	
	/* Determine the total number of target functions. If there are no
	 * target functions, do nothing. No need to allocate memory in this 
	 * case.
	 * */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		*pnum_funcs += entry->payload->repl_table.num_addrs;
	}
	
	printk(KERN_INFO "[cp_controller] "
		"total number of target functions is %u.\n",
		*pnum_funcs);
		
	*ptarget_funcs = kzalloc((*pnum_funcs) * sizeof(void*), GFP_KERNEL);
	*prepl_funcs = kzalloc((*pnum_funcs) * sizeof(void*), GFP_KERNEL);
	if (*ptarget_funcs == NULL || *prepl_funcs == NULL)
	{
		/* Don't care which of the two has failed, kfree(NULL) is 
		 * almost a no-op anyway*/
		free_repl_table(ptarget_funcs, prepl_funcs, pnum_funcs);
		return -ENOMEM;		
	}
	
	i = 0;
	/*list_for_each(pos, &payload_modules)
	{
		unsigned int k;
		entry = list_entry(pos, struct payload_module_list, list);
		BUG_ON(	entry->payload == NULL || 
			entry->payload->orig_addrs == NULL || 
			entry->payload->repl_addrs == NULL);
		
		for (k = 0; k < entry->payload->num_addrs; ++k)
		{
			(*ptarget_funcs)[i] = 
				entry->payload->orig_addrs[k];
			(*prepl_funcs)[i] = 
				entry->payload->repl_addrs[k];
			++i;
		}
	}*/
	
	return 0;
}

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
	
	static const unsigned char op = 0xe8; /* 'call <offset>' */
	
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
		printk( KERN_WARNING "[cp_controller] "
	"Instruction decoder stopped past the end of the section.\n");
	}
		
/* This call may be overkill as insn_get_length() probably has to decode 
 * the instruction completely.
 * Still, to operate safely, we need insn_get_opcode() before we can access
 * c_insn->opcode. 
 * The call is cheap anyway, no re-decoding is performed.
 */
	insn_get_opcode(c_insn); 
	if (c_insn->opcode.value != op)
	{
		/* Not a 'call' instruction, nothing to do. */
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
		printk( KERN_WARNING "[cp_controller] At 0x%p: "
	"opcode: 0x%x, "
	"immediate field is %u rather than 32 bits in size; "
	"insn.length = %u, insn.imm = %u, off_immed = %d\n",
			kaddr,
			(unsigned int)c_insn->opcode.value,
			8 * (unsigned int)c_insn->immediate.nbytes,
			c_insn->length,
			(unsigned int)c_insn->immediate.value,
			insn_offset_immediate(c_insn));
		
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
			
			printk( KERN_INFO "[cp_controller] At 0x%p: "
		"changing address 0x%p to 0x%p (displ: 0x%x to 0x%x)\n",
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
	unsigned int i;
	void* pos = NULL;
	
	/* TODO: provide assert-like wrapper */
	BUG_ON(kbeg == NULL);
	BUG_ON(kend == NULL);
	BUG_ON(kend < kbeg);
		
	pos = kbeg;
	for (i = 0; ; ++i)
	{
		unsigned int len;
		unsigned int k;

		len = do_process_insn(&c_insn, pos, kend,
			from_funcs, to_funcs, nfuncs);
		if (len == 0)	
		{
			printk( KERN_WARNING "[cp_controller] "
			"do_process_insn() returned 0\n");
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

		if (pos >= kend)
		{
			break;
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
		printk( KERN_INFO "[cp_controller] Module \"%s\", "
		"processing \"init\" area\n",
			module_name(mod));
			
		do_process_area(mod->module_init, 
			mod->module_init + mod->init_text_size,
			orig_addrs,
			repl_addrs,
			num_addrs);
	}

	printk( KERN_INFO "[cp_controller] Module \"%s\", "
		"processing \"core\" area\n",
		module_name(mod));
		
	do_process_area(mod->module_core, 
		mod->module_core + mod->core_text_size,
		orig_addrs,
		repl_addrs,
		num_addrs);
	return;
}

/* ================================================================== */
// Module filter.
// Should return not 0, if detector should watch for module with this name.
static int 
filter_module(const char *mod_name)
{
	/* We are interested only in analysing the module which name
	 * is specified in 'target' parameter. 
	 * */
	return strcmp(mod_name, target_name) == 0;
}
// There are 3 functions, which should do real work
// for interaction with modules
static void 
on_module_load(struct module *mod)
{
	struct payload_module_list* entry;
	struct list_head *pos;
	int ret;
	
	printk(KERN_INFO "[cp_controller] Module '%s' has just loaded.\n",
		module_name(mod));
	
	target_in_init = 1;
	
	/* Call try_module_get for all registered payload modules to 
	 * prevent them from unloading while the target is loaded */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		BUG_ON(entry->payload->mod == NULL);
		
		printk(KERN_INFO "[cp_controller] "
			"try_module_get() for payload module '%s'.\n",
			module_name(entry->payload->mod));
		
		if(try_module_get(entry->payload->mod) == 0)
		{
			printk(KERN_ALERT "[cp_controller] "
		"try_module_get() failed for payload module '%s'.\n",
				module_name(entry->payload->mod));
		}
	}
	
	/* Create the combined replacement table */
	ret = create_repl_table(&orig_addrs, &repl_addrs,
		&num_addrs);
	if (ret != 0)
	{
		printk(KERN_ALERT "[cp_controller] "
	"Not enough memory to create the combined replacement table.\n");
		return;
	}
	
	replace_calls_in_module(mod);
	return;
}

/* [NB] This function is called even if initialization of the target module 
 * fails.
 * */
static void 
on_module_unload(struct module *mod)
{
	struct payload_module_list* entry;
	struct list_head *pos;
	
	printk(KERN_INFO "[cp_controller] Module '%s' is going to unload.\n",
		module_name(mod));
	
	/* Destroy the combined replacement table. It is not very efficient 
	 * to destroy / recreate it each time but still it is simple and 
	 * less error-prone. 
	 * ("Premature optimization is the root of all evil" - D.Knuth?).
	 * */
	free_repl_table(&orig_addrs, &repl_addrs, 
		&num_addrs);
	
	/* Release the payload modules as the target is about to unload and 
	 * will execute no code from now on.
	 * 
	 * The payload modules can now unload as well if the user wishes so.
	 * */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		BUG_ON(entry->payload->mod == NULL);
		
		printk(KERN_INFO "[cp_controller] "
			"module_put() for payload module '%s'.\n",
			module_name(entry->payload->mod));
		module_put(entry->payload->mod);
	}
	
	target_in_init = 0; 
	return;
}

/* A callback function to catch loading and unloading of module. 
 * Sets target_module pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module *mod = (struct module *)vmod;

	//switch on module state
	/* TODO: protect from simultaneous execution (would it be enough to
	 * protect 'target_module' variable only?)*/
	switch(mod_state)
	{
	case MODULE_STATE_COMING:// module has just loaded
		if(!filter_module(module_name(mod))) break;
		
		BUG_ON(target_module != NULL);
		target_module = mod;
		on_module_load(mod);
		break;
	
	case MODULE_STATE_GOING:// module is going to unload
		/* if the target module has already been unloaded, 
		 * target_module is NULL, so (mod != target_module) will
		 * be true. */
		if(mod != target_module) break;
		
		on_module_unload(mod);
		target_module = NULL;
	}
	return 0;
}

/* ================================================================ */
// struct for watching for loading/unloading of modules.
struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,
	.priority = 3, /*Some number*/
};
/* ================================================================ */

static void
controller_cleanup_module(void)
{
	unregister_module_notifier(&detector_nb);
	
	/* The system won't let unload the controller while at least one
	 * payload module is loaded: payload modules use symbols from the 
	 * controller.
	 * So, if we managed to get here, there must be no payload modules 
	 * registered.
	 *  */
	BUG_ON(!list_empty(&payload_modules));
	
	/* Even if a target module is now loaded, it must not have been
	 * instrumented as there are no payload modules at the moment.
	 * Note that a payload module cannot be unloaded if there is a 
	 * target module present. So there is no need to uninstrument 
	 * the target module: it was never instrumented. It was probably 
	 * loaded with no payload modules present. 
	 * */

	printk(KERN_INFO "[cp_controller] Cleanup successful\n");
	return;
}

/* ================================================================ */
static int __init
controller_init_module(void)
{
	int result;
	printk(KERN_INFO "[cp_controller] Initializing\n");

	/* Initialize the list */
	INIT_LIST_HEAD(&payload_modules);	

	result = register_module_notifier(&detector_nb);
	if (result < 0)
	{
		goto fail;
	}

	return 0; /* success */

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

/* Look for a given element in the list. */
static struct payload_module_list* 
payload_find(struct kedr_payload* payload)
{
	struct payload_module_list* entry;
	struct list_head *pos;
	
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		if(entry->payload == payload)
			return entry;
	}
	return NULL;
}

/* ================================================================ */
/* Implementation of public functions                               */
/* ================================================================ */

int 
kedr_payload_register(struct kedr_payload* payload)
{
	struct payload_module_list* new_elem = NULL;
	
	BUG_ON(payload == NULL);
	
	/* If there is a target module already watched for, do not allow
	 * to register another payload. */
	if (target_module != NULL)
	{
		return -EBUSY;
	}
	
	if (payload_find(payload) != NULL)
	{
		printk(KERN_ALERT "[cp_controller] Module \"%s\" attempts "
			"to register the same payload twice\n",
			module_name(payload->mod));
		return -EINVAL;
	}
	
	printk(KERN_INFO "[cp_controller] Registering payload "
			"from module \"%s\"\n",
			module_name(payload->mod));
	
	new_elem = kzalloc(sizeof(struct payload_module_list), GFP_KERNEL);
	if (new_elem == NULL) return -ENOMEM;
		
	INIT_LIST_HEAD(&new_elem->list);
	new_elem->payload = payload;
	
	list_add_tail(&new_elem->list, &payload_modules);
	return 0;
}
EXPORT_SYMBOL(kedr_payload_register);

void 
kedr_payload_unregister(struct kedr_payload* payload)
{
	struct payload_module_list* doomed = NULL;
	
	BUG_ON(payload == NULL);
	
	/* By this time, the target module must have been unloaded. */
	BUG_ON(target_module != NULL);
	
	doomed = payload_find(payload);
	if (doomed == NULL)
	{
		printk(KERN_ALERT "[cp_controller] Module \"%s\" attempts "
		"to unregister the payload that was never registered\n",
			module_name(payload->mod));
		return;
	}
	
	printk(KERN_INFO "[cp_controller] Unregistering payload "
			"from module \"%s\"\n",
			module_name(payload->mod));
	
	list_del(&doomed->list);
	kfree(doomed);
	return;
}
EXPORT_SYMBOL(kedr_payload_unregister);

int
kedr_target_module_in_init(void)
{
	if (target_in_init)
	{
		/* Ensure that nobody silently unloads the target 
		 * while we are accessing it here. */
		if (target_module && try_module_get(target_module))
		{
			target_in_init = 
				(target_module->module_init != NULL);
			module_put(target_module);
		}
	}
	
	/* [NB] When the target is unloaded, on_module_unload() will set 
	 * target_in_init to 0.*/
	return target_in_init;
}
EXPORT_SYMBOL(kedr_target_module_in_init);

/* ================================================================ */
