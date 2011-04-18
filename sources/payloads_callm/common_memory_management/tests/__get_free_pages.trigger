[group]
function.name = __get_free_pages
trigger.code =>>
    unsigned long addr = __get_free_pages(GFP_KERNEL, 4);
    free_pages(addr, 4);
<<
