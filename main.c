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
    

if(File_Create("/file1") < 0)
        printf("Can't create file\n");
    else
        printf("File created successfully\n");

if(File_Create("/file1") < 0)
        printf("Can't create file\n");
    else
        printf("File created successfully\n");

 if(Dir_Create("/dir1") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(Dir_Create("/dir1") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(Dir_Create("/dir1/dir2") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(Dir_Create("/dir1/dir2/dir3") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(File_Create("/dir1/dir2/foo.txt") < 0)
        printf("cant create file\n");
    else
        printf("file created successfully\n");    

	char buf[1024];
	memset(buf,'a',sizeof(buf)); 

  char fn[30] = "/file1";
  int fd = File_Open("/file1");
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  if(File_Write(fd, buf, 900) != 900)
    printf("ERROR: can't write 900 bytes to fd=%d\n", fd);
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);

  fd = File_Open("/file2");
  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  if(File_Write(fd, buf, 900) != 900)
    printf("ERROR: can't write 900 bytes to fd=%d\n", fd);
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);

  memset(buf,'0',sizeof(buf));


  if(fd < 0) printf("ERROR: can't open file '%s'\n", fn);
  else printf("file '%s' opened successfully, fd=%d\n", fn, fd);
  File_Read(fd,buf,612);
  memset(buf,0,sizeof(buf));
  
  if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
  else printf("fd %d closed successfully\n", fd);



  char p[16] = "/dir1";
    int sz = Dir_Size(p);
    char* buf1 = (char*)malloc(sz);
      int entries = Dir_Read(p, buf1, sz);
      if(entries < 0) {
        printf("ERROR: can't list '%s'\n", p);
        return -3;
      }
      
      printf("directory '%s':\n     %-15s\t%-s\n", p, "NAME", "INODE");
      int idx = 0;
      for(int i=0; i<entries; i++) {
        printf("%-4d %-15s\t%-d\n", i, &buf1[idx], *(int*)&buf1[idx+16]);
        idx += 20;
      }
      free(buf1);
   

  
  if(File_Unlink("/file1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);


  if(File_Unlink("/a/a/file") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

  if(Dir_Unlink("/dir1") < 0) {
  printf("ERROR: can't remove directory '%s'\n", fn);
  
  }
  else printf("directory '%s' removed successfully\n", fn);

  if(Dir_Unlink("/dir1/dir2/dir3") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

  if(File_Unlink("/dir1/dir2/foo.txt") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

  if(Dir_Unlink("/dir1/dir2") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

  if(Dir_Unlink("/dir1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);
  
  if(Dir_Unlink("/dir1") < 0) {
    printf("ERROR: can't remove file '%s'\n", fn);
    
  }
  else printf("file '%s' removed successfully\n", fn);

 


  if(FS_Sync() < 0) {
    printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
    return -1;
  } else printf("file system sync'd to file '%s'\n", argv[1]);
   

   return 0;

}