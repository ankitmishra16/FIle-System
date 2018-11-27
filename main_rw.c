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
    
    char buf[1024];
	memset(buf,'a',sizeof(buf)); 

  int fd = File_Open("/file111");
  if(fd < 0) printf("ERROR: can't open file \n");
  else printf("file  opened successfully, fd=%d\n",fd);

  char fn[30] = "/file1";
  fd = File_Open("/file1");
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  if(File_Write(fd, buf, 900) != 900)
    printf("ERROR: can't write 900 bytes to fd=%d\n", fd);
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);
  char fn2[30] = "/file2";
  fd = File_Open("/file2");
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn2);
  else printf("file '%s' opened successfully, fd=%d\n", fn2, fd);
  if(File_Write(fd, buf, 900) != 900)
    printf("ERROR: can't write 900 bytes to fd=%d\n", fd);
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);
  
  char buf1[612];
  memset(buf1,'0',sizeof(buf1));
  fd = File_Open("/file1");
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  File_Read(fd,buf1,612);
  memset(buf1,0,sizeof(buf1));
  
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);

  if(FS_Sync() < 0) {
    printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
    return -1;
  } else printf("file system sync'd to file '%s'\n", argv[1]);


}