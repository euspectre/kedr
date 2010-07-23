/*********************************************************************
 * A module to test registration / deregistration of a payload.
 *********************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h> /* __kmalloc */
#include <linux/errno.h>

#include <kedr/base/common.h>

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
repl___kmalloc(size_t size, gfp_t flags)
{
    /* Do nothing special, just call the target function */
	return __kmalloc(size, flags);
}
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* orig_addrs[] = {
	(void*)&__kmalloc
};

/* Addresses of the replacement functions */
static void* repl_addrs[] = {
	(void*)&repl___kmalloc
};

static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,
	.repl_table.orig_addrs  = &orig_addrs[0],
	.repl_table.repl_addrs  = &repl_addrs[0],
	.repl_table.num_addrs   = ARRAY_SIZE(orig_addrs)
};

static struct kedr_payload payload_other = {
	.mod                    = THIS_MODULE,
	.repl_table.orig_addrs  = &orig_addrs[0],
	.repl_table.repl_addrs  = &repl_addrs[0],
	.repl_table.num_addrs   = ARRAY_SIZE(orig_addrs)
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

static void
kedr_test_cleanup_module(void)
{
	return;
}

static int __init
kedr_test_init_module(void)
{
    int result;
    
	BUG_ON(	ARRAY_SIZE(orig_addrs) != 
		ARRAY_SIZE(repl_addrs));
	
    result = doTestRegister();
    
    /* Whether the test passes of fails, initialization of the module 
     * should succeed except if an invalid value has been passed for 
     * "scenario_number" parameter.
     */
    return result;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
