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



void should_fail_with_trap13_because_we_write_on_protected_page(void){
    stack = pmalloc();
    protect_page(stack);
    int* hello = (int*)stack;
    *hello = 4;
    pfree(stack);
}

int main(int argc, char *argv[]){
    printf(1,"$$$$$$$$$$$$$$$$$$$$$$ START SANITY2 ######################\n" );


    printf(1,"should_fail_with_trap13_because_we_write_on_protected_page\n");
    should_fail_with_trap13_because_we_write_on_protected_page();

    sleep(10);
    exit();
}

