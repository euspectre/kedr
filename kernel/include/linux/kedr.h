/* Common definitions for KEDR. */

#ifndef _LINUX_KEDR_H
#define _LINUX_KEDR_H

#include <linux/kedr_local.h>
#include <asm/kedr.h>

#define KEDR_PREFIX "kedr: "

struct kedr_module_map;
extern struct kedr_module_map *current_modmap;
char *kedr_resolve_address(unsigned long addr,
			   struct kedr_module_map *modmap);

void kedr_create_modmap(void);
void kedr_free_modmap(void);
void kedr_modmap_on_coming(struct module *mod);

extern unsigned long kedr_stext;
extern unsigned long kedr_etext;

#endif /* _LINUX_KEDR_H */
