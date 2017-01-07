#ifndef _LINUX_KEDR_LOCAL_H
#define _LINUX_KEDR_LOCAL_H

struct kedr_local
{
	/* Address of a memory block or an ID of some object. */
	unsigned long addr;

	/* Size of a memory block. */
	unsigned long size;

	/*
	 * PC, program counter. Address of an instruction that triggered
	 * the event (or an address of some instruction near it).
	 */
	unsigned long pc;

	/* More info about the event. */
	unsigned int event;
};

#endif /* _LINUX_KEDR_LOCAL_H */
