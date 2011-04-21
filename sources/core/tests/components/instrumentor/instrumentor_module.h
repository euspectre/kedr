#ifndef KEDR_INSTRUMENTOR_MODULE_H
#define KEDR_INSTRUMENTOR_MODULE_H

#include <linux/module.h>

void test_function(int value);
int instrument_module(struct module* m);
void instrument_module_clean(struct module* m);

#endif
