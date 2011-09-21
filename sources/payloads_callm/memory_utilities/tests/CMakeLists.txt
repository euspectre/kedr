# Unfortunately, we do not currently provide call interception tests 
# for the following functions. If these functions are to be intercepted
# on a particular system, they will probably be. 
# The main problem here is how to create a reasonably simple code to use
# these functions in the correct environment and how to properly clean up
# after that. As soon as this is resolved the tests will be provided.

# TODO: prepare tests for the following functions:
set(functions_TODO
	"add_to_page_cache_lru"
	"add_to_page_cache_locked"
)

set(functions_to_test ${functions})
list(REMOVE_ITEM functions_to_test ${functions_TODO})

kedr_test_add_ci_test("memory_utilities" ${functions_to_test})
