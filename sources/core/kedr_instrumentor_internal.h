#ifndef KEDR_INSTRUMENTOR_INTERNAL_H
#define KEDR_INSTRUMENTOR_INTERNAL_H

/*
 * Interface of KEDR instrumentation of the kernel module functionality.
 */

#include <linux/module.h> /* 'struct module' definition */

/*
 * Define pair original function -> real replacement function
 * Both functions should have same signature.
 */
struct kedr_instrumentor_replace_pair
{
    void* orig;
    void* repl;
};


/*
 * Initialize and destroy KEDR instrumentor functionality.
 */
int kedr_instrumentor_init(void);
void kedr_instrumentor_destroy(void);

/*
 * Replace given functions in given module.
 */

int kedr_instrumentor_replace_functions(struct module* m,
    const struct kedr_instrumentor_replace_pair* replace_pairs);

/*
 * Free internal resources which was used for module.
 * 
 * Note: module 'm' is not returned to its state before replace.
 * So, its code should not be used after this.
 */
void kedr_instrumentor_replace_clean(struct module* m);

#endif /* KEDR_INSTRUMENTOR_INTERNAL_H */
