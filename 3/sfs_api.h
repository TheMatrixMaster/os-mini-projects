/** @file sfs_api.h
 *  @brief Global variables, datatypes and function prototypes for the file system.
 *
 *  This contains the prototypes for the file
 *  system api and any macros, constants, datatypes
 *  or global variables you will need.
 *
 *  @author Stephen Z. Lu (thematrixmaster)
 *  @bug No known bugs.
 */

#ifndef SFS_API_H
#define SFS_API_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk_emu.h"

/**  @brief MACROS
    MAX_FILENAME => set 60 bytes as the max filename size
    DISK_NAME => set the diskname to use for our filesystem

    BLOCK_SIZE => set 1024 bytes per block
    NUM_INODES => set 128 inodes (1st inode belongs to root dir)
    NUM_FILE_INODES => helper const to get number of inodes for files
    NUM_DIRECT_POINTERS => each inode has 12 direct pointers

    PTR_SIZE => get pointer size (4 bytes for unsigned int)
    NUM_POINTERS_IN_INDIRECT =>
        Our indirect pointer will point to an intermediate data block which will itself be filled with 
        as many pointers as possile. Thus, we want to calculate the number of pointers we can fit in
        1 block plus the indirect pointer that connect the inode to this intermediate block. 

    MAX_DATA_BLOCKS_PER_FILE => max number of data blocks that 1 inode can point to using all ptrs
    MAX_DATA_BLOCKS_TOTAL => 
        Max number of data blocks is a fictional number that represents the total number of data blocks
        required if we were to fill all inodes with the maximum number of data blocks per inode.
        This is equal to the number of file inodes times the max data blocks per inode.
    MAX_DATA_BLOCKS_SCALED_DOWN => 
        We cannot realistically have MAX_DATA_BLOCKS_TOTAL data blocks in our filesystem, so we will
        scale that number down by a given factor.
        
    NUM_INODE_BLOCKS => number of blocks needed to fit all 128 inodes
    NUM_DATA_BLOCKS_FOR_DIR =>
        We compute the number of data blocks that the directory table will take up. This is equal to the 
        size of a directory entry times the maximum number of files it can contain (# file inodes) divided
        by the block size.
    NUM_DATA_BLOCKS_FOR_BITMAP =>
        This is the number of blocks required to fit our bitmap array that keeps track of free data blocks.
        This is equal to the size of a bitmap entry times the number of data blocks divided by the block size.
    NUM_TOTAL_BLOCKS =>
        We can now calculate the total number of blocks that our filesystem will occupy using the values computed above.

    DATA_BLOCKS_OFFSET =>
        This is the starting address in the disk for storing our data blocks. This starting address is offset
        by the superblock, inode blocks, and directory table blocks.
    BITMAP_BLOCK_OFFSET =>
        We want to store the bitmap at the end of the disk, so we need to calculate the offset of blocks
        that comes before the bitmap. This is equal to the address after we store the data blocks
*/

#define MAX_FILENAME 60
#define DISK_NAME "thematrixmaster.disk"

#define BLOCK_SIZE 1024
#define NUM_INODES 128
#define NUM_FILE_INODES (NUM_INODES - 1)
#define NUM_DIRECT_POINTERS 12

#define PTR_SIZE sizeof(unsigned int)
#define NUM_POINTERS_IN_INDIRECT (BLOCK_SIZE / PTR_SIZE + 1)

#define MAX_DATA_BLOCKS_PER_FILE  (12 + NUM_POINTERS_IN_INDIRECT)
#define MAX_DATA_BLOCKS_TOTAL (NUM_FILE_INODES * MAX_DATA_BLOCKS_PER_FILE)
#define MAX_DATA_BLOCKS_SCALED_DOWN (MAX_DATA_BLOCKS_TOTAL / 16)

#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SIZE + 1)
#define NUM_DATA_BLOCKS_FOR_DIR (sizeof(directory_entry_t) * NUM_FILE_INODES / BLOCK_SIZE + 1)
#define NUM_DATA_BLOCKS_FOR_BITMAP ((sizeof(bitmap_entry_t) * MAX_DATA_BLOCKS_SCALED_DOWN) / BLOCK_SIZE + 1)
#define NUM_TOTAL_BLOCKS (1 + NUM_DATA_BLOCKS_FOR_DIR + NUM_INODE_BLOCKS + MAX_DATA_BLOCKS_SCALED_DOWN + NUM_DATA_BLOCKS_FOR_BITMAP)

#define DATA_BLOCKS_OFFSET (1 + NUM_DATA_BLOCKS_FOR_DIR + NUM_INODE_BLOCKS)
#define BITMAP_BLOCK_OFFSET (DATA_BLOCKS_OFFSET + MAX_DATA_BLOCKS_SCALED_DOWN)


/** @brief Data structure for Superblock
 * occupies 20 bytes and stores
 * metadata about the file system
*/
typedef struct {
    unsigned int magic;
    unsigned int block_size;
    unsigned int fs_size;
    unsigned int inode_table_len;
    unsigned int root_dir_inode;
} superblock_t;

/** @struct i-node occupies 64 bytes and stores:
 * mode: indicates if file is opened
 * link_cnt: indicates if inode is taken
 * size: total size of file contents in bytes
 * direct: array of direct data block pointers
 * indirect: single indirect data block pointer
*/
typedef struct {
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int size;
    unsigned int direct[NUM_DIRECT_POINTERS];
    unsigned int indirect;
} inode_t;

/** @struct directory table entry 
 * occupies 64 bytes and stores a duplicate
 * of the i-node mode field and a char array
 * for the filename
*/
typedef struct {
    unsigned int mode;
    char names[MAX_FILENAME];
} directory_entry_t;

/** @struct file descriptor
 * occupies 16 bytes and stores a ref
 * to the inode and the file's current
 * read-write address
*/
typedef struct {
    int inode;
    uint64_t rwptr;
} file_descriptor_t;

/** @struct bitmap entry 
 * is simply an unsigned 
 * char that can be set to 0 or 1
*/
typedef unsigned char bitmap_entry_t;

void mksfs(int fresh);
int sfs_getnextfilename(char* fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char* name);
int sfs_fclose(int fileID);
int sfs_fwrite(int fileID, const char* buf, int length);
int sfs_fread(int fileID, char* buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char* file);

#endif
