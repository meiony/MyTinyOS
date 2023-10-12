#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"


int main(void) {
    put_str("I am kernel\n");
    init_all();

    void* addr = get_kernel_pages(5);
    put_str("\n get_kernel_page start vaddr is: ");
    put_int((uint32_t) addr);
    put_str("\n");
    
    //ASSERT(1==2);
    while (1);
    return 0;
}