//libfs
//test comment
//test comment vivek
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
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

//to calculate power, used in first_unused_bitmap()
int ipow(int base, int exp) {
    int result = 1;
    for (;;) //unconditional for-loop
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }
    return result;
}

//checking for valid characters
static int valid_character(char c) {
    int temp = (int)c; //casted to an int to compare with ascii decimal value

    if((temp>=48 && temp<=57) || (temp>=65 && temp<=90) || (temp>=97 && temp<=122) || (temp>=45 && temp<=46) || temp==96)
        return 1;

    return 0; // illegal character
}

//return 1 if illegal 0 otherwise
static int illegal_filename(char* name) {
    if(strlen(name)>MAX_NAME-1) // check for legal file length
        return 1;

    int i;
    for(i=0; i<strlen(name); i++) { // check for legal file name characters
        if(valid_character(name[i])==0)
            return 1;
    }
    return 0;
}



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

//function to return first unused bit from bitmap, which starts from sector 'start', 
//spanned over 'num' number of sectors, and total size of bitmap 'nbits'
static int first_unused_bit( int start, int num, int nbytes )
{
    char buf[ SECTOR_SIZE ];
    int sector = 0;
    int bytes_left = nbytes, check = SECTOR_SIZE;

    int rt;//to return the serial number of bit which is 0

    while( sector < num )// to check number of sectors on which bitmap is spanned
    {
        Disk_Read( start + sector*SECTOR_SIZE, buf);

        if( bytes_left < SECTOR_SIZE )//to check if remaining bitmap bytes are less than sector 
            check = bytes_left;       //then set check to that so that that much bytes would be checked only

        for( int i = 0; i < check; i++ )
        {
            char byte_buffer = buf[i];

            if( byte_buffer != (char)255 )
            {
                unsigned char mask = ~byte_buffer;//bitwise complement

                int loc = 0;//to track the location where is first 0

                while( mask != 0 )
                {
                    loc++;
                    mask = mask>>1;
                }

                mask = ipow(2, loc - 1);

                buf[i] = buf[i] | mask ;

                Disk_Write( start + sector*SECTOR_SIZE, buf );

                return (sector * SECTOR_SIZE * 8) + (i*8) + (8 - loc );//( full sectors) + ( bytes full in current sector) + ( bits full in current byte)
                    //we are not adding one here because indexing starts from 0  
            }
        }


        bytes_left-=SECTOR_SIZE;// SECTOR_SIZE bytes have been red 
        sector++;// go to next sectro
    } 

    return -1;//bitmap is full

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
    printf("___ parent inode = %d\n", parent_inode);

    int parent_sector = INODE_TABLE_START_SECTOR + parent_inode / INODES_PER_SECTOR ;//to calculate sector of parent inode
    int  parent_offset = parent_inode - ( parent_sector - INODE_TABLE_START_SECTOR ) * INODES_PER_SECTOR;//to calculate position of inode on current sector

    char buf[SECTOR_SIZE];
    Disk_Read( parent_sector, buf );//to read the sector of parent's inode

    inode_t* parent = (inode_t*)( buf + ( parent_offset*sizeof(inode_t) ) );
    printf("___ load parent inode: parent_inode = %d, inode address %p, %d (size=%d, type=%d)\n",parent_inode ,parent, parent_inode, parent->size, parent->type);

    //inode_t* parent = get_inode(parent_inode);
    //printf("___ load parent inode: parent_inode = %d, inode address %p, %d (size=%d, type=%d)\n",parent_inode ,parent, parent_inode, parent->size, parent->type);
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
                    printf("___ found child inode %d\n", child_inode);

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

    //to remove first '/' from path
    char tgt[ MAX_PATH ];
    strncpy( tgt, path + 1, MAX_PATH - 1 );
    tgt[ MAX_PATH - 1] = '\0';
    printf("___ path copied\n");
    char *target = tgt;
    //to initialize parent and child inode number, initially root i.e., 0
    int parent = -1, child = 0;

    char *follow;//to access parts of path, as a/b/c/d.txt then a,b,c will be accessed seperately by follow
    while( (follow = strsep( &target, "/")) != NULL )
    {
        if( *follow == '\0') continue;//to check if the whole path is accessed,now strsep will return NULL so loop will break

        if( illegal_filename( follow ) )//chceking if path-part is legal
        {
            return -1;
        }
        if( child < 0 )
        {
            return -1;
        }

        parent = child;//pushing the child inode to parent, to go further in child's directory, so making it parent
        child = get_child_inode(parent, follow);
        if(last_fname) strcpy(last_fname, follow);
    }

    if( child < -1 ) return -1;//some error happend
    else
    {
        if( parent == -1 && child == 0 ) parent = 0;

        *last_inode = child;

        return parent;
    }

}

// add a new file or directory (determined by 'type') of given name
// 'file' under parent directory represented by 'parent_inode'
int add_inode(int type, int parent_inode, char* file)
{
    int child_inode_number = first_unused_bit( INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, INODE_BITMAP_SIZE );

    if( child_inode_number < 0 )
    {
        printf("___ inode is not available");
        return -1;
    }
    printf("___ child inode is available with inode number %d \n", child_inode_number );

    int new_inode_sector = INODE_TABLE_START_SECTOR + child_inode_number/INODES_PER_SECTOR;//to calculate sector number on which it lie
    char buf[ SECTOR_SIZE ];//buffer to read the sector where new inode/ parent inode lies
    if(Disk_Read( new_inode_sector, buf ) < 0 ) return -1;//read the sector on which new inode lies

    int sector_num = new_inode_sector - INODE_TABLE_START_SECTOR;//sector from start on which new inode lies
    int offset = child_inode_number - sector_num * INODES_PER_SECTOR ;//going to byte where new inode to be stored

    inode_t* child_inode = (inode_t*)(buf + offset*sizeof(inode_t));
    for(int i = 0 ;i<30;i++)
        child_inode->data[i]=0;
    memset(child_inode, 0, sizeof(inode_t) );
    child_inode->type = type;
    if( Disk_Write( new_inode_sector, buf) < 0 ) {
        printf("___ Disk write failed returning -1\n");
        return -1;
    }//writing back the new inode's entry

    // Retrieving parents inode to make entry

    memset( buf, 0, SECTOR_SIZE );//clearing buffer buf to reuse it
    int parent_inode_sector = INODE_TABLE_START_SECTOR + parent_inode/INODES_PER_SECTOR;//to calculate sector number on which the parent inode lie
    if(Disk_Read( parent_inode_sector, buf ) < 0 ) {
        printf("___ Disk read failed returning -1\n");
        return -1;
    }//read the sector on which new inode lies

    sector_num = parent_inode_sector - INODE_TABLE_START_SECTOR;//number of sectors from start on which parent inode lies
    offset = parent_inode - sector_num * INODES_PER_SECTOR ;//going to byte where parent inode to be stored

    inode_t* parent = (inode_t*)(buf + offset*sizeof(inode_t));

    if( parent->type != 1 ) {
        printf("___ parent not directory returning -2\n");
        return -2;
    }//Parent is not directory

        //Calculations for  dirent_t of  directory
    int number_of_entries = parent->size;
    int sector_sub = number_of_entries/DIRENTS_PER_SECTOR;
    char dirent_buf[ SECTOR_SIZE ];

    if( ( sector_sub * DIRENTS_PER_SECTOR) == number_of_entries )// New sector is needed as rest sectors are full
    {
        if( sector_sub == 30) {
            printf("___ sector sub is 30 returning -1\n");
            return -1;
        }//Parent directory is full with its capacity to have subdirectories/files

        int new_sector = first_unused_bit( SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE );//get the sector number for new sector
        parent->data[sector_sub] = new_sector ;//making entry in parent's inode for new sector
        memset( dirent_buf, 0, SECTOR_SIZE );//filling dirent buffer with 0, because we had created neww
                                            //new secctor, and we will write dirent buffer on that sector
                                            // so it should be clear, unless it may write some garbage 
        printf("___ New sector is created,with sector number %d\n", new_sector);
    }
    else
    {
        if( Disk_Read( parent->data[sector_sub], dirent_buf) < 0 ) {
            printf("___ Disk read second failed returning -1\n");
            return -1;
        }

        printf("___ Sector loaded with sector number %d, for group number \n", parent->data[sector_sub], sector_sub );
    }

    int offset_sub = parent->size - sector_sub * DIRENTS_PER_SECTOR;//to calculate where the new entry to be made

    dirent_t* sub = ( dirent_t*)( dirent_buf + offset_sub * sizeof(dirent_t) );//tofetch  out the dirent structure from dirent buf

    sub->inode = child_inode_number;//to make inode entry in dirent structure
    strncpy( sub->fname, file, MAX_NAME );//to make file_name entry in dirent structre

    if( Disk_Write( parent->data[sector_sub], dirent_buf) < 0 ) {
        printf("___ Disk write second failed returning -1\n");
        return -1;
    }

    parent->size++;

    if( Disk_Write( parent_inode_sector, buf) < 0 ) {
        printf("___ Disk write third failed returning -1\n");
        return -1;
    }

    return 0;
}

// used by both File_Create() and Dir_Create(); type=0 is file, type=1
// is directory
int create_file_or_directory(int type, char* pathname)
{
  int child_inode;
  char last_fname[MAX_NAME];
  int parent_inode = follow_path(pathname, &child_inode, last_fname);
  if(parent_inode >= 0) {
    if(child_inode >= 0) {
      printf("___ file/directory '%s' already exists, failed to create\n", pathname);
      osErrno = E_CREATE;
      return -1;
    } else {
      if(add_inode(type, parent_inode, last_fname) >= 0) {
    printf("___ successfully created file/directory: '%s'\n", pathname);
    return 0;
      } else {
    printf("___ error: something wrong with adding child inode\n");
    osErrno = E_CREATE;
    return -1;
      }
    }
  } else {
    printf("___ error: something wrong with the file/path: '%s'\n", pathname);
    osErrno = E_CREATE;
    return -1;
  }
}


// helper function to unlink the file from the parent node
int unlink_helper(int parent_inode,int child_inode) {
    char inode_buffer[SECTOR_SIZE];
    int inode_sector = INODE_TABLE_START_SECTOR+parent_inode/INODES_PER_SECTOR;
    if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
    printf("___ load inode table for parent inode %d from disk sector %d\n",
            parent_inode, inode_sector);

    // parent inode
    int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    int offset = parent_inode-inode_start_entry;
    assert(0 <= offset && offset < INODES_PER_SECTOR);
    inode_t* parent = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
    printf("___ get parent inode %d (size=%d, type=%d)\n",
            parent_inode, parent->size, parent->type);

    if(parent->size > 0)
        parent->size--;
    if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
    printf("___ update parent inode on disk sector %d\n", inode_sector);

    int i;
    for(i = 0; i < 30; i++) {
        int sector_data = parent->data[i];
        char sector_buffer[SECTOR_SIZE];

        Disk_Read(sector_data, sector_buffer);

        int j;
        for(j = 0; j < 25; j++){
            dirent_t * entry = (dirent_t*)(sector_buffer + (j*20)); //get all the data from the sector

            if(entry->inode == child_inode){
                memset(entry, 0, sizeof(dirent_t));
                Disk_Write(sector_data, sector_buffer);
                return 0;
            }
        }
    }
    return -1; // error when unlinking
}


// reset 'num_bit'th bit with 'num' sectors starting at 'start' sector
//0 = success -1 = failure
static int reset_bitmap(int start, int num, int num_bit) {

    int max_bits = SECTOR_SIZE * 8;
    int temp_bit = num_bit - 1;
    int bit_num = temp_bit%max_bits; // bit location on "last" sector
    int sector_location = (temp_bit/max_bits) + start; // temp_bit/max_bits is always going to be 0??

    if(num_bit > max_bits)
        return -1; // ibit is not in scope

    printf("num value: %d\n", num);
    printf("max_bits values: %d\n", max_bits);
    printf("start value from bitmap_reset: %d\n", start);
    printf("sector location from bitmap_reset: %d\n", sector_location);

    if(bit_num==0) {
        sector_location -= 1;
        bit_num = max_bits;
    }


    char buffer[SECTOR_SIZE];
    Disk_Read(sector_location, buffer);

    int byte_location = bit_num/8;
    int bit_location = bit_num%8;

    int temp_buffer = buffer[byte_location];
    int mask = 255 - ipow(2, 7-bit_location);

    buffer[byte_location] = temp_buffer & mask;
    Disk_Write(sector_location, buffer);

    return 0;

}


//check if file or directory 0 if directory 1 if file
int get_path_type(char* pathname) {
    int token;
    char filename[MAX_NAME];
    follow_path(pathname, &token, filename); // find the token

    if(token==-1) // if token is invalid
        return -1;

    inode_t* inode = get_inode(token); // get inode of this token
    return inode->type; // return the type of this token
}


int remove_inode(int type, int parent_inode, int child_inode) {

    //for file
    if(type == 0) {
        char buffer[SECTOR_SIZE];
        inode_t* file = get_inode(child_inode);

        for(int i = 0; i < 30; i++) {
            int file_sector = (unsigned char) file->data[i];

            if(file_sector != 0) {
                memset(buffer, 0, SECTOR_SIZE);
                Disk_Write(file_sector, buffer);
                reset_bitmap(2, 3, file_sector + 1);
            }
        }

        reset_bitmap(1, 1, child_inode);
        unlink_helper(parent_inode, child_inode);
        return 0; //success
    }
    //directory
    if (type == 1) {
        unlink_helper(parent_inode, child_inode);
        return 0; //success
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
                if(Disk_Write(INODE_TABLE_START_SECTOR + i, buffer) == -1) {
                    printf("_____ inode initialize failed\n");
                    osErrno = E_GENERAL;
                    return -1;
                }
            }
            inode_t* parent = get_inode(0);
            printf("___  in load parent inode: parent_inode = %d, inode address %p, %d (size=%d, type=%d)\n",0 ,parent, 0, parent->size, parent->type);

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
    
    printf("File_Create('%s'):\n", file);
    return create_file_or_directory(0, file);
}

int File_Open(char *file)
{
    printf("FS_Open '%s'\n", file);
    //first unused file descriptor
    int fd;
    bool found = false;
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
       if(open_files[i].inode == 0) {
           fd = i;
           found = true;
           break;
       }
    }
    if(!found) {
        printf("___ max files already open\n");
        osErrno = E_TOO_MANY_OPEN_FILES;
        return -1;
    }

    int child_inode;
    follow_path(file, &child_inode, NULL); //retrieves child inode number of 'file'
    if (child_inode == -1) {
        printf("___ file '%s' not found\n", file);
        osErrno = E_NO_SUCH_FILE;
        return -1;
    }
    else {
        int inode_sector = INODE_TABLE_START_SECTOR + child_inode/INODES_PER_SECTOR;
        char buffer[SECTOR_SIZE];
        if(Disk_Read(inode_sector, buffer) == -1) {
            printf("___ cant read inode for file '%s' from disk sector\n");
            osErrno = E_GENERAL;
            return -1;
        }
        printf("___ disk read success, loading inode table entry for for inode from disk sector\n");

        int inode_start = (inode_sector - INODE_TABLE_START_SECTOR) * INODES_PER_SECTOR;
        int offset = child_inode - inode_start;
        assert(offset >= 0 && offset < INODES_PER_SECTOR);
        inode_t* child = (inode_t*) (buffer + offset* sizeof(inode_t));
        printf("___ inode %d, size = %d, type = %d", child_inode, child->size, child->type);

        if(child->type != 0) {
            printf("___ FILE ERROR - '%s' not a file\n", file);
            osErrno = E_GENERAL;
            return -1;
        }

        //all correct. initialize file entries in open file table
        open_files[fd].inode = child_inode;
        open_files[fd].size = child->size;
        open_files[fd].pos = 0;
        return fd; //file descriptor returned

    }

}

int
File_Read(int fd, void *buffer, int size)
{
    printf("FS_Read\n");
    /* YOUR CODE */
    int remaining_bytes = size; // the bytes remaining to write
    int initial_position = open_files[fd].pos; // initial position to start write
    printf("___ Initial Position of pos variable is : %d\n",initial_position);
    int start = open_files[fd].pos / SECTOR_SIZE; // current location of file pointer
    int end = ((size+open_files[fd].pos+SECTOR_SIZE-1)/SECTOR_SIZE); // end point after write

    printf("___ Start : %d  End : %d Size: %d\n",start,end,size);

    // error checking

    if(open_files[fd].inode < 1){ // if the file is not open
        osErrno = E_BAD_FD;
        return -1;
    }


    inode_t* inode = get_inode(open_files[fd].inode); // grab file inode

    if(inode->size < size)
    {
        printf("...File size exceeded! \n");
        return -1;
    }

    if(open_files[fd].pos %SECTOR_SIZE==0)
        open_files[fd].pos = 0;

    printf("___ Inode number --%d\n",open_files[fd].inode);
    printf("___ File Inode size :: %d \n",inode->size);
    int position = 0;
    for(int i=start; i<end; i++) { // write everything byte by byte
        // char data_buffer[SECTOR_SIZE];
        char disk_buffer[SECTOR_SIZE];
        int sector = inode->data[i];
        printf("Sector number being read is : %d\n",sector);

        if(open_files[fd].pos == 0){
            // if all the remaining bytes don't fit in one sector
            if(remaining_bytes >= SECTOR_SIZE){ // buffer gets as much as possible
                Disk_Read(sector, disk_buffer);
                memcpy((buffer+position),disk_buffer,SECTOR_SIZE);
                position += SECTOR_SIZE;
                printf("___ Full sector:: %d %.*s\n", sector, SECTOR_SIZE, disk_buffer);
                remaining_bytes -= SECTOR_SIZE;
            } else{ // buffer gets everything
                Disk_Read(sector, disk_buffer);

                memcpy((buffer+position),disk_buffer,remaining_bytes);
                position += remaining_bytes;
                printf("remaining_bytes:: %d %.*s\n", sector, remaining_bytes, disk_buffer);
                remaining_bytes = 0;
            }
            // write data to sector
        } else{
            Disk_Read(sector, disk_buffer);
            // printf("File pointer from middle:: %s",disk_buffer);
            position += open_files[fd].pos%SECTOR_SIZE;
            printf("From the middle:: %d :: %d  :: %.*s\n", sector, position,SECTOR_SIZE-position, disk_buffer+position);
            memcpy((buffer+position), disk_buffer, SECTOR_SIZE-open_files[fd].size);

            remaining_bytes -= SECTOR_SIZE-open_files[fd].pos;

            open_files[fd].pos = 0;
        }
    }

    File_Seek(fd,size);

    printf("My buffer%s\n",buffer );


    return -1;
}

int
File_Write(int fd, void *buffer, int size)
{
    printf("FS_Write\n");
    int remaining_bytes = size; // the bytes remaining to write
    int initial_position = open_files[fd].pos; // initial position to start write
    // dprintf("\nInitial Position of pos variable is : %d\n",initial_position);
    int start = open_files[fd].pos / SECTOR_SIZE; // current location of file pointer
    int end = ((size+open_files[fd].pos+SECTOR_SIZE-1)/SECTOR_SIZE); // end point after write


    // error checking
    if(fd<0 && fd>MAX_OPEN_FILES) { // not enough space on the disk
        osErrno = E_NO_SPACE;
        return -1;
    } else if(open_files[fd].inode < 1){ // if the file is not open
        osErrno = E_BAD_FD;
        return -1;
    } else if(end > 29){ // file exceeds maximum file size
        osErrno = E_FILE_TOO_BIG;
        return -1;
    }

    inode_t* inode = get_inode(open_files[fd].inode); // grab file inode

    printf("___ Start : %d  End : %d Size: %d\n",start,end,inode->size);

    int i;

    int child_inode = open_files[fd].inode;

    int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
    char inode_buffer[SECTOR_SIZE];
    if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;

    // get the child inode
    int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    int offset = child_inode-inode_start_entry;
    assert(0 <= offset && offset < INODES_PER_SECTOR);
    inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));


    for(i=start; i<end; i++) { // write everything byte by byte
        // char data_buffer[SECTOR_SIZE];
        char disk_buffer[SECTOR_SIZE];
        char disk_data[2000];
        int sector = inode->data[i];


        if(sector==0) // find location where the information should be stored
        {
            sector = first_unused_bit(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
            inode->data[i]= sector;
            child->data[i]=sector;
            printf("___ Sector number to be written at is : %d\n",inode->data[i]);
            open_files[fd].pos = 0;
        }

        if(open_files[fd].pos == 0){
            // if all the remaining bytes don't fit in one sector

            // sector = bitmap_first_unused(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
            // inode->data[i]= sector;
            // child->data[i]=sector;

            if(remaining_bytes >= SECTOR_SIZE){ // buffer gets as much as possible
                memcpy(disk_buffer, (buffer + i*SECTOR_SIZE), SECTOR_SIZE);

                remaining_bytes -= SECTOR_SIZE;
            } else{ // buffer gets everything
                memcpy(disk_buffer, (buffer + i*SECTOR_SIZE), remaining_bytes);
                remaining_bytes = 0;
            }
            printf("___ XXXXXSector Number to which data is written :: %d\n",sector);
            printf("___ Data written :: %s\n",disk_buffer);

            Disk_Write(sector, disk_buffer); // write data to sector
            Disk_Read(sector, disk_data);
        } else{
            Disk_Read(sector, disk_buffer);
            memcpy(disk_buffer, (disk_buffer+open_files[fd].size), SECTOR_SIZE-open_files[fd].size);
            Disk_Write(sector, disk_buffer);
            printf("___ Data written :: %s\n",disk_buffer);
            remaining_bytes -= SECTOR_SIZE-open_files[fd].size;
            open_files[fd].pos = 0;
        }

    }
    //  printf("Inode number --%d\n",open_files[fd].inode);
    // for(i=0;i<30;i++)
    //  printf(":: %d  ",inode->data[i]);
    // printf("\n");


    inode->size = initial_position+size;
    open_files[fd].pos = initial_position+size; // move end file pointer to the end
    open_files[fd].size = initial_position+size;
    // load the disk sector containing the child inode

    // update the new child inode and write to disk

    child->size = initial_position+size;
    printf("%d\n",child->size);
    if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
    printf("... update child inode %d (size=%d, type=%d), update disk sector %d\n",
            child_inode, child->size, child->type, inode_sector);

    File_Seek(fd,size);

    return size;
}

int File_Seek(int fd, int offset)
{
    printf("FS_Seek\n");
    bool is_open = false;
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(open_files[i].inode == open_files[fd].inode) {
            if(open_files[fd].size < offset) {
                osErrno = E_SEEK_OUT_OF_BOUNDS;
                return -1;
            }
            open_files[fd].pos = offset; //position updated
            return open_files[fd].pos;
        }
    }
    osErrno = E_BAD_FD;
    return -1;
}

int File_Close(int fd)
{
    printf("FS_Close\n");
    //bound check
    if (fd < 0 || fd > MAX_OPEN_FILES) {
        printf("___ file descriptor '%d' out of bound\n", fd);
        osErrno = E_BAD_FD;
        return -1;
    }
    //check if opened or not
    if (open_files[fd].inode <= 0) {
        printf("___ file with fd '%d' not open\n", fd);
        osErrno = E_BAD_FD;
        return -1;
    }
    //close file
    open_files[fd].inode = 0;
    printf("___ file with fd '%d' closed successfully\n");
    return 0;

}

int File_Unlink(char *file)
{
    printf("FS_Unlink\n");
    int child_inode;
    char filename[MAX_NAME];
    int parent_inode = follow_path(file, &child_inode, filename); // finds the parent

    if(child_inode < 1){ // if file does not exist
        osErrno = E_NO_SUCH_FILE;
        return -1; // file is not deleted
    }
    bool is_open = false;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if(open_files[i].inode == child_inode) {
            is_open = true;
            break;
        }
    }
    if(is_open){ // if file is in use
        osErrno = E_FILE_IN_USE;
        return -1; // file is not deleted
    }
    if(remove_inode(0, parent_inode, child_inode) >= 0){ // file can be deleted
        return 0;
    }
    osErrno = E_GENERAL;
    return -1;
}


// directory ops
int
Dir_Create(char *path)
{
    printf("Dir_Create %s\n", path);
    return create_file_or_directory(1, path);
}

int
Dir_Size(char *path)
{
    printf("Dir_Size\n");
    int byte_counter = 0;
    if(get_path_type(path)==1) { // if this is a directory
        int token;
        char filename[MAX_NAME];
        follow_path(path, &token, filename); // find the location of token
        inode_t* directory_inode = get_inode(token); // get the inode of token

        printf("___ Inode Number received is : %d\n",token );
        int i;
        for(i=0; i<30; i++){ // for all 30 allocated data blocks per file
            int sector = directory_inode->data[i]; // grab data from inode
            char sector_buffer[SECTOR_SIZE];
            Disk_Read(sector, sector_buffer); // save data onto sector_buffer

            int j;
            for(j=0; j<25; j++){
                dirent_t* entry = (dirent_t*)(sector_buffer + (j*20)); // multiplied by 20 because each entry contains 16-byte names + 4-byte integer inode numbers
                if(entry->inode>0)
                    byte_counter+=20; // each entry is 20 bytes
            }
        }
        printf("___ Byte Counter1 : %d\n", byte_counter);
        return byte_counter; // return total byte count
    }
    printf("___ Byte Counter2 : %d\n", byte_counter);
    return 0; // files do not have any bytes referred to by path
}

int
Dir_Read(char *path, void *buffer, int size)
{
    printf("Dir_Read\n");
    int byte_counter = 0;
    if(get_path_type(path)==1) {
        int token;
        char filename[MAX_NAME];
        follow_path(path, &token, filename); // find location of token

        int directory_size = Dir_Size(path);
        printf("___ directory size: %d\n", directory_size);

        if(size < directory_size) { // size cannot contain all entries
            osErrno = E_BUFFER_TOO_SMALL;
            return -1;
        }

        printf("___ \t%-15s\t%-s\n", "NAME", "INODE");
        inode_t* inode = get_inode(token);
        if(inode->type == 1){ // if it's a directory
            int i;
            for(i=0; i<30; i++) { // for all 30 allocated data blocks per file
                int sector =inode->data[i];
                char sector_buffer[SECTOR_SIZE];
                Disk_Read(sector, sector_buffer);

                int j;
                for(j=0; j<25; j++) {
                    dirent_t* entry = (dirent_t*)(sector_buffer + (j*20));

                    if(entry->inode > 0)
                        printf("___ %-4d\t%-15s\t%-d\n", j, entry->fname, entry->inode);
                }
            }
        }
        return byte_counter;
    }
    return -1;
}

int
Dir_Unlink(char *path)
{
    printf("Dir_Unlink\n");
    if(!strcmp(path, "/") != 0) { // directory does not exist
        osErrno = E_GENERAL;
        return -1;
    }
    else if (strcmp(path, "/") == 0) { // can't unlink the root
        osErrno = E_ROOT_DIR;
        return -1;
    }
    else if(get_path_type(path) == 1) { // if path is a directory
        int token;
        char filename[MAX_NAME];
        int parent_inode = follow_path(path, &token, filename);

        inode_t* inode = get_inode(token); // get inode of token
        if(inode->size > 0){ // there are still files within the directory
            osErrno = E_DIR_NOT_EMPTY;
            return -1;
        }
        else if(remove_inode(1, parent_inode, token) >= 0){ // all clear; remove directory
            return 0;
        }
    }
    return -1;
}
