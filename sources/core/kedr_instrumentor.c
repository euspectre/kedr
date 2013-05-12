/*
 * The "instrumentor" component of KEDR system. 
 * Its main responsibility is to instrument the target module
 * to allow call interception.
 */
 
/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h> /* 'struct module' definition */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/hash.h> /* hash_ptr definition */

#include <kedr/core/kedr.h>
#include <kedr/asm/insn.h>       /* instruction decoder machinery */

#include "kedr_instrumentor_internal.h"
#include "config.h"

/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "kedr_instrumentor: "

/* ================================================================ */
//MODULE_AUTHOR("Eugene A. Shatokhin");
//MODULE_LICENSE("GPL");

/* Save target module for implement target_in_init() */
static struct module* target_module;
/* ================================================================ */

/* Nonzero if the target module is initializing, 0 if it has completed 
 * initialization or if no target is loaded at the moment. */
static int target_in_init = 0;

/* A spinlock to protect target_in_init from concurrent access. 
 * A mutex would not do because target_in_init can be accessed from atomic
 * context too (kedr_target_module_in_init() can be called from a replacement
 * function executing in atomic context).
 */
static DEFINE_SPINLOCK(target_in_init_lock);

/* ================================================================ */

/* Replacement table as a hash table */
struct repl_elem
{
	struct hlist_node list;
	void* orig;
	void* repl;
};

struct repl_hash_table
{
	struct hlist_head* heads;
	unsigned int bits;
};

static void
repl_hash_table_destroy(struct repl_hash_table* table)
{
	int i;
	size_t hash_table_size = 1 << table->bits;
	struct hlist_head *heads = table->heads;
	
	if (heads == NULL)
		return;
	
	for (i = 0; i < hash_table_size; i++)
	{
		struct hlist_node *tmp;
		struct repl_elem *elem;

		kedr_hlist_for_each_entry_safe(elem, tmp, &heads[i], list) {
			hlist_del(&elem->list);
			kfree(elem);
		}
	}
	
	kfree(table->heads);
	table->heads = NULL;
	table->bits = 0;
}

static int 
repl_hash_table_init_from_array(struct repl_hash_table* table,
	const struct kedr_instrumentor_replace_pair* array)
{
	int result;
	
	size_t hash_table_size;
	unsigned int bits;
	struct hlist_head *heads = NULL;
	
	size_t array_size = 0;
	const struct kedr_instrumentor_replace_pair* repl_pair;
	for(repl_pair = array; repl_pair->orig != NULL; repl_pair++)
	{
		array_size++;
	}
	
	hash_table_size = (array_size * 10 + 6)/ 7;
	
	bits = 0;
	for(; hash_table_size >= 1; hash_table_size >>= 1)
		bits++;
	if(bits == 0) bits = 1;
	
	hash_table_size = 1 << bits;
	
	heads = kzalloc(hash_table_size * sizeof(*heads), GFP_KERNEL);
	if(heads == NULL)
	{
		pr_err("Failed to allocate hash table for replacement functions.");
		return -ENOMEM;
	}
	
	table->heads = heads;
	table->bits = bits;
	
	for (repl_pair = array; repl_pair->orig != NULL; repl_pair++)
	{
		struct hlist_head* head;
		struct repl_elem* elem;
		
		head = &heads[hash_ptr(repl_pair->orig, bits)];
		
		kedr_hlist_for_each_entry(elem, head, list)
		{
			if(elem->orig == repl_pair->orig)
			{
				pr_err("Cannot replace function twice.");
				result = -EINVAL;
				goto free_table;
			}
		}
		elem = kmalloc(sizeof(*elem), GFP_KERNEL);
		if(elem == NULL)
		{
			pr_err("Fail to allocate replace table's element.");
			result = -ENOMEM;
			goto free_table;
		}
		elem->orig = repl_pair->orig;
		elem->repl = repl_pair->repl;
		hlist_add_head(&elem->list, head);
	}
	return 0;

free_table:
	repl_hash_table_destroy(table);	
	return result;
}

static void*
repl_hash_table_get_repl(struct repl_hash_table* table, void* orig)
{
	struct hlist_head* head;
	struct repl_elem* elem;
	
	head = &table->heads[hash_ptr(orig, table->bits)];
	
	kedr_hlist_for_each_entry(elem, head, list)
	{
		if(elem->orig == orig)
		{
			return elem->repl;
		}
	}
	return NULL;
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
	struct repl_hash_table* repl_table)
{
	/* ptr to the 32-bit offset argument in the instruction */
	u32* offset = NULL; 
	
	/* address of the function being called */
	void* addr = NULL;
	void* repl_addr; 
	
	static const unsigned char op_call = 0xe8; /* 'call <offset>' */
	static const unsigned char op_jmp  = 0xe9; /* 'jmp  <offset>' */
	
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
	repl_addr = repl_hash_table_get_repl(repl_table, addr);
	if (repl_addr != NULL)
	{
		/* Change the address of the function to be called */
		*offset = CALL_OFFSET_FROM_ADDR(
			kaddr, 
			c_insn->length,
			repl_addr
		);
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
	struct repl_hash_table* repl_table)
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
			repl_table);
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
 * being analyzed. 
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
replace_calls_in_module(struct module* mod,
	struct repl_hash_table* repl_table)
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
			repl_table);
	}

	KEDR_MSG(COMPONENT_STRING 
		"target module: \"%s\", processing \"core\" area\n",
		module_name(mod));
		
	do_process_area(mod->module_core, 
		mod->module_core + mod->core_text_size,
		repl_table);
	return;
}

/*
 * on_module_load() should do real work when the target module is loaded:
 * instrument it, etc.
 *
 * Note that this function is called with instrumentor_mutex locked.
 */
int kedr_instrumentor_replace_functions(struct module* m,
	const struct kedr_instrumentor_replace_pair* replace_pairs)
{
	int result;
	unsigned long flags;
	
	struct repl_hash_table repl_hash_table;
	
	result = repl_hash_table_init_from_array(&repl_hash_table, replace_pairs);
	if (result)
	{
		pr_err("Failed to create hash table of replacements.");
		return result;
	}
	
	spin_lock_irqsave(&target_in_init_lock, flags);
	target_in_init = 1;
	target_module = m;
	spin_unlock_irqrestore(&target_in_init_lock, flags);

	replace_calls_in_module(m, &repl_hash_table);
	
	repl_hash_table_destroy(&repl_hash_table);
	return 0;
}

void kedr_instrumentor_replace_clean(struct module* m)
{
	unsigned long flags;
   
	spin_lock_irqsave(&target_in_init_lock, flags);
	target_in_init = 0; 
	target_module = NULL;
	spin_unlock_irqrestore(&target_in_init_lock, flags);
	
	return;
}

/* ================================================================ */

/* Returns nonzero if a target module is currently loaded and it executes 
 * its init function at the moment, 0 otherwise (0 is returned even if there
 * is no target module loaded at the moment).
 * 
 * In fact, the function just checks whether the target module has already
 * dropped its ".init.*" sections (which the modules do after they have 
 * completed their initialization). Therefore the function will always 
 * return 0 if the init function was not marked as "__init" in the target 
 * module. This should not be a big problem.
 * 
 * This function can be useful to implement particular fault simulation 
 * scenarios (like "fail everything after init"), etc.
 * 
 * Note however that there is a chance that the target module will complete
 * its initialization after kedr_target_module_in_init() has determined that
 * the target is in init but before the return value of 
 * kedr_target_module_in_init() is used. It is up to the user of the target
 * module to ensure that no request is made to the module until its 
 * initialization is properly handled by the tests.
 *
 * It is allowed to call this function from atomic context.
 */
int
kedr_target_module_in_init(void)
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
		BUG_ON(target_module == NULL);
		target_in_init = (target_module->module_init != NULL);
	}
	result = target_in_init;
	spin_unlock_irqrestore(&target_in_init_lock, flags);
	
	/* [NB] When the target is unloaded, on_module_unload() will set 
	 * target_in_init to 0.*/
	return result;
}

/* ================================================================ */
int
kedr_instrumentor_init(void)
{
	return 0;
}
void
kedr_instrumentor_destroy(void)
{
}

/* ================================================================ */
