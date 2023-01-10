#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;

  // Fetch the nth 32-bit system call argument.
  argint(0,&mask);
  if(mask < 0){
    return -1;
  }
  
  myproc()->trace_mask = mask; 
  return 0;
}

uint64
sys_sysinfo(void)
{
//   sysinfo needs to copy a struct sysinfo back to user space; see sys_fstat() (kernel/sysfile.c) and filestat() (kernel/file.c) for examples of how to do that using copyout().
// To collect the amount of free memory, add a function to kernel/kalloc.c
// To collect the number of processes, add a function to kernel/proc.c

// struct file *f;
//   uint64 st; // user pointer to struct stat

//   argaddr(1, &st);
//   if(argfd(0, 0, &f) < 0)
//     return -1;
//   return filestat(f, st);

  struct sysinfo p;
  p.freemem = get_free_memory();
  p.nproc = get_unused_process();
  uint64 addr;
  argaddr(0,&addr);
  // argaddr(0,p);
  if(copyout(myproc() -> pagetable,addr,(char *)&p,sizeof(struct sysinfo))<0){
    return -1;
  }
  return 0;
}