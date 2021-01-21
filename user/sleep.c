#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
  int i = 0;

  if(argc < 2){
    fprintf(2, "Usage: sleep time...\n");
    exit(1);
  }

  for(i = 0;i < argc - 1;i++){
    sleep(atoi(argv[i+1]));
  }

  exit(0);
}
