#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

#define PGSIZE 4096
char * stack;



void protect_page_should_fail_because_page_was_malloc_allocated(void){
    sbrk(5);
    sbrk(3);
    stack = malloc(1024);
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");
    stack = malloc(2048);
    stack = malloc(3076);
    stack = malloc(4096);   
    stack = malloc(1024+4096);
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");     
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");
    stack = malloc(3076+4096);
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");   
    stack = malloc(2048+ 2*4096);      
    stack = pmalloc();
    if((int)stack % 4096)
        printf(1,"not aligned\n");              

}
void pfree_should_fail_becaused_page_was_malloc_allocated(void){
    stack = malloc(PGSIZE);
    if (pfree(stack)==1){
        printf(1,"fail: pfree succedeed on malloced space\n");
    }
    free(stack);
    printf(1,"passed\n");
}
void pfree_should_fail_because_page_is_not_protected(void){
    stack = pmalloc();
    if (pfree(stack)==1)
        printf(1,"fail: pfree failed on unprotected space\n");
    free(stack);
    printf(1,"passed\n");
}
void should_fail_with_trap13_because_we_write_on_protected_page(void){
    stack = pmalloc();
    protect_page(stack);
    int* hello = (int*)stack;
    *hello = 4;
    pfree(stack);
}
void should_work_because_page_is_not_protected(void){
    stack = pmalloc();
    int* hello = (int*)stack;
    *hello = 4;
    free(stack);
    printf(1,"passed\n");
}
void should_work_because_page_is_only_being_read(void){
    stack = pmalloc();
    int* hello = (int*)stack;
    *hello = 4;
    protect_page(stack);
    int* hello_read = (int*)stack;
    if (*hello_read==4)
        printf(1,"passed\n");
    else{
        printf(1,"failed: could not read from protected page\n");
    }
}
char *mem;

char * getMemoryAt(int i){
    return mem + i * PGSIZE;
}


void Task2_test(void){




}


int main(int argc, char *argv[]){
    printf(1,"$$$$$$$$$$$$$$$$$$$$$$ START SANITY ######################\n" );

    //trying_to_read_from_a_protected_page();
    printf(1,"starting protect_page_should_fail_because_page_was_malloc_allocated\n");
    protect_page_should_fail_because_page_was_malloc_allocated(); // "ppage failed"

    printf(1,"pfree_should_fail_becaused_page_was_malloc_allocated\n");
    pfree_should_fail_becaused_page_was_malloc_allocated(); // "pfree failed"

    printf(1,"pfree_should_fail_because_page_is_not_protected\n");
    pfree_should_fail_because_page_is_not_protected(); // "pfree failed"

//    printf(1,"should_fail_with_trap13_because_we_write_on_protected_page\n");
//    should_fail_with_trap13_because_we_write_on_protected_page();

    printf(1,"should_work_because_page_is_not_protected\n");
    should_work_because_page_is_not_protected();
    printf(1,"should_work_because_page_is_only_being_read\n");
    should_work_because_page_is_only_being_read();

//    fork_copied_correctly();
    exit();
}

