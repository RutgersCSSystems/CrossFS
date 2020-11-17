all:multi_page_read shared_multi_page_read create_pool single_page_read_test flush_cache asm_flush clear_persist

clear_persist:
	gcc clear_persist.c -o clear_persist

multi_page_read:
	gcc mmap_read_multi_page_test.c -o multi_page_read

single_page_read_test:
	gcc mmap_read_single_page_test.c -o single_page_read_test

shared_multi_page_read:
	gcc shared_read_multi_page_test.c -o shared_multi_page_read

flush_cache:
	gcc flush_cache.c -o flush_cache

create_pool:
	gcc create_pool.c -o create_pool

asm_flush:
	gcc asm_flush.c -o asm_flush
	
clean:
	rm -f multi_page_read
	rm -f shared_multi_page_read
	rm -f create_pool
	rm -f single_page_read_test
	rm -f flush_cache
	rm -f asm_flush
	rm -f clear_persist
