unsigned long addr = get_zeroed_page(GFP_KERNEL);
free_pages(addr, 0);