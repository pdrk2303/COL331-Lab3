#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

/* System Call Definitions */
int 
set_sched_policy(void)
{
    // Implement your code here 
    // return -1;
    int policy;

    if (argint(1, &policy) < 0) {
        return -22;
    }

    if (policy != 0 && policy != 1) {
        return -22;
    }

    myproc()->policy = policy;
    return 0;
}

int 
get_sched_policy(void)
{
    // Implement your code here 
    // return -1;
    return myproc()->policy;
}
