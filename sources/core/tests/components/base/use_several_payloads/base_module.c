/*
 * Module envelope around kedr_base.
 */

/* ========================================================================
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include "kedr_base_internal.h"
#include <kedr/core/kedr.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(kedr_payload_register);
EXPORT_SYMBOL(kedr_payload_unregister);

EXPORT_SYMBOL(kedr_base_target_load_callback);
EXPORT_SYMBOL(kedr_base_target_unload_callback);

static int __init
base_module_init(void)
{
    return kedr_base_init(NULL);
}

static void __exit
base_module_exit(void)
{
    kedr_base_destroy();
}

module_init(base_module_init);
module_exit(base_module_exit);