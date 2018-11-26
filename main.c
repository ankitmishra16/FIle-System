#include <stdio.h>

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
    if (argc != 2) {
	usage(argv[0]);
    }
    char *path = argv[1];

    FS_Boot(path);
    FS_Sync();
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

    if(Dir_Create("/dir1/dir2") < 0) {
        printf("cant create directory\n");
    }
    else {
        printf("directory created successfully\n");
    }

    return 0;
}
