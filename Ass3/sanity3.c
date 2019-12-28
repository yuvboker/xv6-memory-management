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



void allocate_all_memory();
char * getMemoryAt(int i);
char *mem;

int number[] =
        {3,
         1,
         2,
         3,
         4,
         5,
         6,
         7,
         8,
         9
        };

int main(int argc, char *argv[]){
    printf(1,"Instruction for sanity3 SCFIFO:\n\nwait for the prompt the show up then press\nCTRL+P to show proc dump\nexpect "
            "to see 19 3 0 1 4 \n");


    allocate_all_memory();
    //allocating 14 pages worth of memory

    printf(1,"press CTRL+P now!!!!\n");
    sleep(1000);
    free(mem);
    exit();
}


int algn_page(char* p){
    return PGSIZE - ((uint)p % PGSIZE);
}
void allocate_all_memory(){
    printf(1,"Allocating all memory...\n");


    mem = malloc(15 * PGSIZE);

    mem = mem + algn_page(mem);

    if((uint)mem % PGSIZE != 0){
        printf(1,"error in allocation in tests...\n");
        exit();
    }
}


char * getMemoryAt(int i){
    return mem + i * PGSIZE;
}
