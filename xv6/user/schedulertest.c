#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILDREN 6

static void
cpu_burn(int loops)
{
  volatile long x = 0;

  for(int i = 0; i < loops; i++)
    x += i;
}

int
main(int argc, char *argv[])
{
  printf("schedulertest: spawning %d children\n", NCHILDREN);

  for(int i = 0; i < NCHILDREN; i++){
    int pid = fork();

    if(pid < 0){
      printf("schedulertest: fork failed\n");
      exit(1);
    }

    if(pid == 0){

      // CPU-bound processes:
      // long CPU bursts.
      if(i % 2 == 0){
        for(int r = 0; r < 30; r++){
          cpu_burn(40000000);
        }
      }

      // Short-burst processes:
      // run briefly, then voluntarily yield.
      else{
        for(int r = 0; r < 30; r++){
          cpu_burn(5000000);
          yield();
        }
      }

      exit(0);
    }
  }

  for(int i = 0; i < NCHILDREN; i++){
    int status;
    int pid = wait(&status);

    printf("schedulertest: child pid=%d finished\n", pid);
  }

  printf("schedulertest: done\n");
  exit(0);
}