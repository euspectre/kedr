/* similar to arch/x86/include/asm/livepatch.h */

#ifndef _ASM_X86_KEDR_H
#define _ASM_X86_KEDR_H

#include <linux/ftrace.h>

static inline void kedr_arch_set_pc(struct pt_regs *regs, unsigned long ip)
{
	regs->ip = ip;
}

#endif
