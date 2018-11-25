//libfs
//test comment
//test comment vivek
#include <stdio.h>
#include <string.h>
#include "LibFS.h"
#include "LibDisk.h"

// global errno value here
int osErrno;


//global values
#define MAX_PATH 256
#define MAX_NAME 16
#define MAX_OPEN_FILES 256
#define MAX_FILES 1000
#define MAX_FILE_SIZE (MAX_SECTORS_PER_FILE*SECTOR_SIZE)
#define MAX_SECTORS_PER_FILE 30
#define MAGIC_NUMBER 7777 //predefined magic number

//initializing all sectors predefined values
#define SUPERBLOCK_START_SECTOR 0                   // superblock containing magic number


#define INODE_BITMAP_START_SECTOR 1                 //bitmap of inodes starting at sector 1
#define INODE_BITMAP_SIZE ((MAX_FILES+7)/8)
#define INODE_BITMAP_SECTORS ((INODE_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE)


#define SECTOR_BITMAP_START_SECTOR (INODE_BITMAP_START_SECTOR+INODE_BITMAP_SECTORS)
#define SECTOR_BITMAP_SIZE ((NUM_SECTORS+7)/8)    //gets ceil value of max bits required for sectors
#define SECTOR_BITMAP_SECTORS ((SECTOR_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE) //total sectors for bitmap of sectors


#define INODE_TABLE_START_SECTOR (SECTOR_BITMAP_START_SECTOR+SECTOR_BITMAP_SECTORS) //start of inode sector
#define INODES_PER_SECTOR (SECTOR_SIZE/sizeof(inode_t))
#define INODE_TABLE_SECTORS ((MAX_FILES+INODES_PER_SECTOR-1)/INODES_PER_SECTOR)


#define DATABLOCK_START_SECTOR (INODE_TABLE_START_SECTOR+INODE_TABLE_SECTORS)


#define DIRENTS_PER_SECTOR (SECTOR_SIZE/sizeof(dirent_t))


static char filesys_name[1024];

/*****************REQUIRED STRUCTURES************************/

//structure for inode
typedef struct inode {
    int size; // the size of the file or number of directory entries
    int type; // 0 regular; 1 directory
    int data[MAX_SECTORS_PER_FILE]; // indices to sectors containing data blocks
} inode_t;


// stores file name corresponding to inode
typedef struct dirent {
    char fname[MAX_NAME]; // name of the file
    int inode; // inode of the file
} dirent_t;

//structure for open file -> open file table
typedef struct open_file {
    int inode; // pointing to the inode of the file (0 means entry not used)
    int size;
    int pos;   // read/write position
} open_file_t;
static open_file_t open_files[MAX_OPEN_FILES];

/***********************END OF REQUIRED STRUCTURES******************/

/**********************START OF HELPER FUNCTIONS***********************/

// Initializing inode bitmap and sector bitmap
//for inode 1st bit is 1 (for root) others = 0
// for sector bitmap 255 bits arre 1 (for superblock(1), inode bitmap(1),
// sector bitmap(3), inode table(250), total = 254)
static void initialize_bitmap() {
    char bitmap_buffer[SECTOR_SIZE];

        bitmap_buffer[0] = (unsigned char) 128;

        for( int i = 1; i < SECTOR_SIZE; i++ )
            bitmap_buffer[i] = 0;

        Disk_Write( 1, bitmap_buffer );//write to the inode bitmap

        int i, j;

        for( i = 0; i < 31; i++ )
            bitmap_buffer[ i ] = ( unsigned char )255;

        bitmap_buffer[ 31 ] = ( unsigned char )254;

        for( i = 32 ; i < SECTOR_SIZE ; i++ )
            bitmap_buffer[ i ] = 0;

        Disk_Write( 2, bitmap_buffer ); //sector bimap

        memset( bitmap_buffer, 0, SECTOR_SIZE );
        Disk_Write( 3, bitmap_buffer ); //sector bitmap
        Disk_Write( 4, bitmap_buffer ); // sector bitmap
}


// helper function to get a specific inode_t from its inode number
inode_t* get_inode(int child_inode) {

    int inode_sector = INODE_TABLE_START_SECTOR + child_inode/INODES_PER_SECTOR; // Caculate sector number which hs inode
    char inode_buffer[SECTOR_SIZE]; // buffer to store the sector's data which contains inode

    Disk_Read(inode_sector, inode_buffer); // save data in inode_sector onto inode_buffer

    int child_loc = child_inode - ( ( inode_sector - INODE_TABLE_START_SECTOR ) * INODES_PER_SECTOR); // Calculating actual position of inode in its sector
    inode_t* child = ( inode_t* )( inode_buffer + child_loc*sizeof( inode_t ) );
  
    return child;
}


//get_child_inode will return inode number of 'fname' file/directory, whhich should be
// in parent_inode i.e., it should be sub-directory or file, if not return -1, 
static int get_child_inode(int parent_inode, char* fname)
{
    int child_inode;//to return the inode number of child node

    int parent_sector = INODE_TABLE_START_SECTOR + parent_inode / INODES_PER_SECTOR ;//to calculate sector of parent inode
    int  parent_offset = parent_inode - ( parent_sector - INODE_TABLE_START_SECTOR ) * INODES_PER_SECTOR;//to calculate position of inode on current sector

    char buf[SECTOR_SIZE];
    Disk_Read( parent_sector, buf );//to read the sector of parent's inode

    inode_t* parent = (inode_t*)( buf + ( parent_offset*sizeof(inode_t) ) );
    printf("___ load parent inode: parent_inode address %p, %d (size=%d, type=%d)\n",parent, parent_inode, parent->size, parent->type);

    if( parent->type == 0 )//0 represents file, and 1 represents directory, in parent->type
    {
        printf("___ Not a directory\n");
        return -2 ;
    }
    else
    {
        int parent_entries = parent->size ;
        int index = 0;
        while( parent_entries > 0 )
        {
            if( Disk_Read( parent->data[index],buf ) < 0 )//using buf again because its previous data is never being used again
                return -2;

            for( int i = 0 ; i < DIRENTS_PER_SECTOR ; i++ )
            {
                if( i > parent_entries )
                    break;
                if(!strcmp(((dirent_t*)buf)[i].fname, fname))
                {
                    child_inode = ((dirent_t*)buf)[i].inode;
                    printf("___ found child inode %d", child_inode);

                    return child_inode;
                }
            }

            parent_entries -= DIRENTS_PER_SECTOR;// every time update number of parent_entries left to check 
            index++;
        }

    }

    return -1;//not in the parent

}

//follow_path will return immediate parent's inode number, file's/directories inode number
// through last_inode argument and name of file/directory thourgh last name
static int follow_path(char* path, int* last_inode, char* last_fname)
{
  if(!path) {
    printf("___ invalid path\n");
    return -1;
  }
  if(path[0] != '/') {
    printf("___ '%s' not absolute path\n", path);
    return -1;
  }

}


/**********************END OF HELPER FUNCTIONS********************************/
int FS_Boot(char *back_file) {
    printf("FS_Boot %s\n", back_file);

    // oops, check for errors
    if (Disk_Init() == -1) {
        printf("Disk_Init() failed\n");
        osErrno = E_GENERAL;
        return -1;
    }
    printf("disk - '%s' initialized\n", back_file);
    strncpy(filesys_name, back_file, 1024);
    filesys_name[1023] = '\0';


    //load disk
    if(Disk_Load(filesys_name) == -1) {
        printf("___ load disk '%s' failed\n", filesys_name);

        if(diskErrno == E_OPENING_FILE) {
            printf("____ cant open file_sys '%s', creating new file system\n", filesys_name);

            //initializing superblock
            char buffer[SECTOR_SIZE];
            memset(buffer, 0, SECTOR_SIZE);
            *(int *) buffer = MAGIC_NUMBER;
            if(Disk_Write(SUPERBLOCK_START_SECTOR, buffer) == -1) {
                printf("_____ superblock initialization failed\n");
                osErrno = E_GENERAL;
                return -1;
            }
            printf("____ superblock initialization successful\n");

            //initialize inode bitmap
            initialize_bitmap();
            printf("____ inode bitmap intialized\n");
            printf("____ sector bitmap intialized\n");

            for(int i = 0; i < INODE_TABLE_SECTORS; i++) {
                memset(buffer, 0, SECTOR_SIZE);
                if(i == 0) {
                    //first inode table entry = root directory
                    ((inode_t *) buffer)->size = 0;
                    ((inode_t *) buffer)->type = 1;
                }
                if(Disk_Write(INODE_TABLE_SECTORS + i, buffer) == -1) {
                    printf("_____ inode initialize failed\n");
                    osErrno = E_GENERAL;
                    return -1;
                }
            }
            printf("____ inode table initialized\n");

            //saving progress
            if(Disk_Save(filesys_name) == -1) {
                printf("_____ disk save failed for '%s'\n", filesys_name);
                osErrno = E_GENERAL;
                return -1;
            }
            else {
                printf("_____ All initialized, Boot Successfull\n");
                memset(open_files, 0, MAX_OPEN_FILES * sizeof(open_file_t));
                return 0;
            }
        }
        // error while reading file
        else {
            printf("___ file read failed for '%s' , boot failed\n", filesys_name);
            osErrno = E_GENERAL;
            return -1;
        }
    }
    else {
        printf("___ load disk from file '%s' successful\n", filesys_name);

        int size = 0;
        FILE *fp = fopen(filesys_name, "r");
        if(fp) {
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fclose(fp);
        }
        if(size != SECTOR_SIZE * NUM_SECTORS) {
            printf("___ file size check for '%s' failed\n", filesys_name);
            osErrno = E_GENERAL;
            return -1;
        }
        printf("___ file size check for '%s' successful\n", filesys_name);

        //check magic number
        bool magic = false;
        char buffer[SECTOR_SIZE];
        if(Disk_Read(SUPERBLOCK_START_SECTOR, buffer) == -1)
            magic = false;
        if(*(int *) buffer == MAGIC_NUMBER)
            magic = true;
        else
            magic = false;

        if(magic) {
            // final boot success
            printf("___ check magic successful\n");
            memset(open_files, 0, MAX_OPEN_FILES*sizeof(open_file_t));
            return 0;
        }
        else {
            printf("... check magic failed, boot failed\n");
            osErrno = E_GENERAL;
            return -1;
        }
    }

    // do all of the other stuff needed...

    return 0;
}

int FS_Sync()
{
    printf("FS_Sync\n");

    //just have to do disk save
    if (Disk_Save(filesys_name) == -1) {
        printf("___ Disk sync for file %s failed\n", filesys_name);
        osErrno = E_GENERAL;
        return -1;
    }
    else {
        printf("___ disk sync successfull for file %s\n", filesys_name);
        return 0;
    }
}


int
File_Create(char *file)
{
    printf("FS_Create\n");
    return 0;
}

int
File_Open(char *file)
{
    printf("FS_Open\n");
    return 0;
}

int
File_Read(int fd, void *buffer, int size)
{
    printf("FS_Read\n");
    return 0;
}

int
File_Write(int fd, void *buffer, int size)
{
    printf("FS_Write\n");
    return 0;
}

int
File_Seek(int fd, int offset)
{
    printf("FS_Seek\n");
    return 0;
}

int
File_Close(int fd)
{
    printf("FS_Close\n");
    return 0;
}

int
File_Unlink(char *file)
{
    printf("FS_Unlink\n");
    return 0;
}


// directory ops
int
Dir_Create(char *path)
{
    printf("Dir_Create %s\n", path);
    return 0;
}

int
Dir_Size(char *path)
{
    printf("Dir_Size\n");
    return 0;
}

int
Dir_Read(char *path, void *buffer, int size)
{
    printf("Dir_Read\n");
    return 0;
}

int
Dir_Unlink(char *path)
{
    printf("Dir_Unlink\n");
    return 0;
}
