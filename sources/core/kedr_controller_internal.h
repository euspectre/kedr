#ifndef KEDR_CONTROLLER_INTERNAL_H
#define KEDR_CONTROLLER_INTERNAL_H

/*
 * Interface of KEDR replacement functionality.
 */

#include <linux/module.h> /* 'struct module' definition */
#include "kedr_base_internal.h" /* struct kedr_replace_real_pair */

/*
 * Initialize and destroy KEDR controller functionality.
 */
int kedr_controller_init(void);
void kedr_controller_destroy(void);

/*
 * Replace given functions in given module.
 */

int kedr_controller_replace_functions(struct module* m,
    struct kedr_replace_real_pair* replace_pairs);

/*
 * Free internal resources which was used for module.
 * 
 * Note: module 'm' is not returned to its state before replace.
 * So, its code should not be used after this.
 */
void kedr_controller_replace_clean(struct module* m);



#endif /* KEDR_CONTROLLER_INTERNAL_H */