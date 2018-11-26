#include <stdio.h>
#include <string.h>
#include "LibFS.h"

void
usage(char *prog)
{
    fprintf(stderr, "usage: %s <disk image file>\n", prog);
    exit(1);
}

int
main(int argc, char *argv[])
{

    char *path = argv[1];

    if(FS_Boot(argv[1]) < 0) {
        printf("ERROR: can't boot file system from file '%s'\n", argv[1]);
        return -1;
    } else printf("file system booted from file '%s'\n", argv[1]);


    if(File_Create("/newfile") < 0)
        printf("cant create file\n");
    else
        printf("file created successfully\n");

    //Error

    if(File_Create("/newfile") < 0)
        printf("cant create file\n");
    else
        printf("file created successfully\n");


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

    if(Dir_Create("/dir1/Second") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(Dir_Create("/dir1/Second/Third") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(File_Create("/dir1/Second/foo.txt") < 0)
        printf("cant create file\n");
    else
        printf("file created successfully\n");


    char buf[1024];
    memset(buf,'1',sizeof(buf));

    int fd = File_Open("/newfile");

    if(fd<0)
        printf("Cant open file \n");
    else printf("Sucess Opening \n");

    if(File_Write(fd,buf,512)<0)
        printf("Error Writing : \n");
    else printf("successfully Written\n");

    if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
    else printf("fd %d closed successfully\n", fd);



    if(Dir_Create("/dir1/Second") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    if(File_Create("/dir1/foo.txt") < 0)
        printf("cant create file\n");
    else
        printf("file created successfully\n");

    fd = File_Open("/dir1/foo.txt");
    if(fd<0)
        printf("Cant open file \n");
    else printf("Sucess Opening \n");

    if(File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
    else printf("fd %d closed successfully\n", fd);


    if(Dir_Create("/dir1/Second/Third") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }
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
    if(FS_Sync() < 0) {
        printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
        return -1;
    } else printf("file system sync'd to file '%s'\n", argv[1]);

    return 0;
}