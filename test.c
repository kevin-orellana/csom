#include <stdio.h>
#include "memlib.h"
#include "mm.h"

void main(){
	printf("size of word / void*: %lu\n", sizeof(void*));
	printf("size of size_t: %lu\n", sizeof(size_t));
	void *mem_sbrk_result = mem_sbrk(100);
	printf("size of mem_sbrk_result: %lu", sizeof(mem_sbrk_result));
	
	return;
}
