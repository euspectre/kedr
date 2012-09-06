/*********************************************************************
 * A module to test registration / deregistration of a payload.
 *********************************************************************/
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
 
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h> /* __kmalloc */
#include <linux/errno.h>

#include <kedr/core/kedr.h>

/*********************************************************************/
MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/

enum ETestScenarios
{
    ERegUnregTwiceOK = 0, /* reg, unreg, reg, unreg for the same payload */
    ERegSameTwice,        /* reg the same payload twice */
    EUnregOther,          /* unreg what was never registered */
    ENumTestScenarios
};

/* "scenario_number" parameter - see enum ETestScenarios above. */
int scenario_number = ENumTestScenarios;
module_param(scenario_number, int, S_IRUGO);

/* "test_passed" - test result, 1 - passed, any other value - failed */
int test_passed = 0; /* failed by default */
module_param(test_passed, int, S_IRUGO);

/*********************************************************************
 * Replacement functions (only for the replacement table not to be empty)
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags,
    struct kedr_function_call_info* call_info)
{
    /* Do nothing special, just call the target function */
	return __kmalloc(size, flags);
}

static void
pre___kmalloc(size_t size, gfp_t flags,
    struct kedr_function_call_info* call_info)
{
    /* Do nothing */
}

/*********************************************************************/

static struct kedr_replace_pair replace_pairs[] =
{
	{
		.orig = (void*)&__kmalloc,
		.replace = (void*)&repl___kmalloc
	},
	{
		.orig = NULL
	}
};

static struct kedr_payload payload = {
	.mod            = THIS_MODULE,

	.replace_pairs	= replace_pairs
};


static struct kedr_pre_pair pre_pairs[] =
{
	{
		.orig = (void*)&__kmalloc,
		.pre = (void*)&pre___kmalloc
	},
	{
		.orig = NULL
	}
};

static struct kedr_payload payload_other = {
	.mod        = THIS_MODULE,

	.pre_pairs	= pre_pairs
};
/*********************************************************************/
static void
testRegUnregTwiceOK(void)
{
    int regErrorCode = 0;
    test_passed = 0;
    
    // the first pass (reg, unreg)
    regErrorCode = kedr_payload_register(&payload);
    if (regErrorCode != 0)
    {
        return; // test failed
    }
    kedr_payload_unregister(&payload);
    
    // the second pass (reg, unreg)
    regErrorCode = kedr_payload_register(&payload);
    if (regErrorCode != 0)
    {
        return; // test failed
    }
    kedr_payload_unregister(&payload);
    
    test_passed = 1;
    return;
}

static void
testRegSameTwice(void)
{
    int regErrorCode = 0;
    test_passed = 0;
    
    regErrorCode = kedr_payload_register(&payload);
    if (regErrorCode != 0)
    {
        return; // test failed
    }
    
    // register another payload - just in case
    regErrorCode = kedr_payload_register(&payload_other);
    if (regErrorCode != 0)
    {
        kedr_payload_unregister(&payload);
        return; // test failed
    }
    
    // attempt to register the payload that is already registered
    regErrorCode = kedr_payload_register(&payload); // must fail
    if (regErrorCode == 0)
    {
        kedr_payload_unregister(&payload);
        kedr_payload_unregister(&payload_other);
        return; // test failed
    }
    
    kedr_payload_unregister(&payload);
    kedr_payload_unregister(&payload_other);

    test_passed = 1;
    return;
}

static void
testUnregOther(void)
{
    int regErrorCode = 0;
    test_passed = 0;
    
    // the first pass
    regErrorCode = kedr_payload_register(&payload);
    if (regErrorCode != 0)
    {
        return; // test failed
    }
    
    // attempt to unregister the payload that has never been registered
    kedr_payload_unregister(&payload_other);
    
    // unregister properly this time
    kedr_payload_unregister(&payload);
    
    // the second pass
    regErrorCode = kedr_payload_register(&payload);
    if (regErrorCode != 0)
    {
        return; // test failed
    }
    kedr_payload_unregister(&payload);
    
    test_passed = 1;
    return;
}

static int
doTestRegister(void)
{
    int result = 0;
    
    switch (scenario_number)
    {
    case ERegUnregTwiceOK:
        testRegUnregTwiceOK();
        break;
        
    case ERegSameTwice:
        testRegSameTwice();
        break;
        
    case EUnregOther:
        testUnregOther();        
        break;
        
    default:
        // Invalid scenario number
        result = -EINVAL;
        break;
    }

    return result;
}

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
kedr_test_cleanup_module(void)
{
	return;
}

static int __init
kedr_test_init_module(void)
{
    int result;
    
    result = functions_support_register();
    if(result) return result;
	
    result = doTestRegister();
    
    functions_support_unregister();

    /* Whether the test passes of fails, initialization of the module 
     * should succeed except if an invalid value has been passed for 
     * "scenario_number" parameter.
     */
    return result;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
