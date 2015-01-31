/*
 * Simple envelop around of the kedr_target_detector.
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

#include "kedr_target_detector_internal.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

/* 
 * Name of the module to analyze. It can be passed to 'insmod' as 
 * an argument, for example,
 *  insmod kedr_target_detector_module.ko target_name="module_to_detect"
 */
static char* target_name = ""; /* an empty name will match no module */
module_param(target_name, charp, S_IRUGO);

static int target_is_loaded = 0;
module_param(target_is_loaded, int, S_IRUGO);

int on_target_load(struct module* m)
{
    target_is_loaded = 1;
    return 0;
}

void on_target_unload(struct module* m)
{
    target_is_loaded = 0;
}

int force_several_targets(void)
{
    return 0;
}

void unforce_several_targets(void)
{
}

static int __init
kedr_module_init(void)
{
    int result;
    
    result = kedr_target_detector_init();
    if(result) return result;
    
    result = kedr_target_detector_set_target_name(target_name);
    if(result)
    {
        kedr_target_detector_destroy();
        return result;
    }

    return 0;
}

static void __exit
kedr_module_exit(void)
{
    kedr_target_detector_destroy();
}

module_init(kedr_module_init);
module_exit(kedr_module_exit);
