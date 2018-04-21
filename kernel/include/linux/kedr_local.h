#ifndef _LINUX_KEDR_LOCAL_H
#define _LINUX_KEDR_LOCAL_H

struct kedr_local
{
	// TODO: re-think which fields are needed here, because the handlers
	// now get more processed data than in the previous implementations.
	// No need to store some of these data here.

	/*
	 * PC, program counter. Address of an instruction that triggered
	 * the event (or an address of some instruction near it).
	 */
	//unsigned long pc;

	/* More info about the event. */
	unsigned int event;
};

#endif /* _LINUX_KEDR_LOCAL_H */
