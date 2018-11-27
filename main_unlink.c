#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LibFS.h"

void err(char *prog)
{
  printf("Error: %s <disk fn needed>\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  if (argc != 2) err(argv[0]);
  
  if(FS_Boot(argv[1]) < 0) {
    printf("ERROR: can't boot file system from file '%s'\n", argv[1]);
    return -1;
    } else printf("File system booted from file '%s'\n", argv[1]);

  char fn[20] = "/file1";
  if(File_Unlink("/file1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

  char fn1[20] = "/a/file";
  if(File_Unlink("/a/file") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn1);
  }
  else printf("file '%s' removed successfully\n", fn1);

  char fn2[20] = "/dir1";
  if(Dir_Unlink("/dir1") < 0) {
  printf("ERROR: can't remove directory '%s'\n", fn2);
  
  }
  else printf("directory '%s' removed successfully\n", fn2);
  char fn3[20] = "/dir1/dir2/dir3";
  if(Dir_Unlink("/dir1/dir2/dir3") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn3);
    
  }
  else printf("file '%s' removed successfully\n", fn3);
  char fn4[20] = "/dir1/dir2/foo.txt";
  if(File_Unlink("/dir1/dir2/foo.txt") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn4);
    
  }
  else printf("file '%s' removed successfully\n", fn4);
  char fn5[20] = "/dir1/dir2";
  if(Dir_Unlink("/dir1/dir2") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn5);
    
  }
  else printf("file '%s' removed successfully\n", fn5);
  char fn6[20] = "/dir1";

  if(Dir_Unlink("/dir1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn6);
    
  }
  else printf("file '%s' removed successfully\n", fn6);
  char fn7[20] = "/dir1";
  if(Dir_Unlink("/dir1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn7);
    
  }
  else printf("file '%s' removed successfully\n", fn7);


  if(FS_Sync() < 0) {
    printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
    return -1;
  } else printf("file system sync'd to file '%s'\n", argv[1]);
   

   return 0;

}