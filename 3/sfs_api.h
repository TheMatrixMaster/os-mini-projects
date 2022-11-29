#ifndef SFS_API_H
#define SFS_API_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk_emu.h"

/*
    BLOCK_SIZE => set 2048 bytes per block
    NUM_INODES => set 193 inodes (1st inode belongs to root dir)
    NUM_FILE_INODES => helper const to get number of inodes for files
    NUM_DIRECT_POINTERS => each inode has 12 direct & 1 indirect pointers

    PTR_SIZE => get block pointer size (4 bytes for unsigned int)
    NUM_POINTERS_IN_INDIRECT =>
        Our indirect pointer will point to an intermediate data block which will itself be filled with 
        as many pointers as possile. Thus, we want to figure out the number of pointers we can fit in
        1 block plus the indirect pointer that connect the inode to this intermediate block. 

    NUM_INODE_BLOCKS => find the number of blocks needed to fit all inodes
    MAX_DATA_BLOCKS_PER_FILE => max number of data blocks that 1 inode can point to using direct and indirect ptrs
    
    NUM_DATA_BLOCKS_FOR_DIR =>
        We compute the number of data blocks that the directory table will take up. This is equal to the 
        size of a directory entry times the maximum number of files it can contain (# file inodes) divided
        by the block size rounded up.

    MAX_DATA_BLOCKS_TOTAL => 
        Max number of data blocks that we will allocate is equal to the max number of data blocks that all non-root-dir inodes can point to.
        This is equal to the number of inodes minus 1 (for root dir) times the max data blocks per inode.

    NUM_BITMAP_ENTRIES =>
        We will keep track of the free data blocks using a list of 64 bit unsigned integers. 
        Each int will cover 64 data blocks, so the number of such integers we will need to cover all data
        blocks will be equal to the total number of free data blocks divided by 64.

    BITMAP_BLOCK_OFFSET =>
        We want to store the bitmap at the end of the disk, so we need to calculate the offset of blocks
        that comes before the bitmap. This is equal to the address after we store the blocks for the root
        directory, inodes, and free data blocks
*/

#define MAX_FILENAME 60
#define DISK_NAME "thematrixmaster.disk"

#define BLOCK_SIZE 1024
#define NUM_INODES 128
#define NUM_FILE_INODES (NUM_INODES - 1)
#define NUM_DIRECT_POINTERS 12

#define PTR_SIZE sizeof(unsigned int)
#define NUM_POINTERS_IN_INDIRECT (BLOCK_SIZE / PTR_SIZE + 1)

#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SIZE + 1)
#define MAX_DATA_BLOCKS_PER_FILE  (12 + NUM_POINTERS_IN_INDIRECT)
#define NUM_DATA_BLOCKS_FOR_DIR (sizeof(directory_t) * NUM_FILE_INODES / BLOCK_SIZE + 1)

#define MAX_DATA_BLOCKS_TOTAL (NUM_FILE_INODES * MAX_DATA_BLOCKS_PER_FILE)
#define MAX_DATA_BLOCKS_SCALED_DOWN (MAX_DATA_BLOCKS_TOTAL / 16)
#define NUM_BITMAP_ENTRIES (MAX_DATA_BLOCKS_SCALED_DOWN / sizeof(bitmap_entry_t))
#define DATA_BLOCKS_OFFSET (1 + NUM_DATA_BLOCKS_FOR_DIR + NUM_INODE_BLOCKS)
#define BITMAP_BLOCK_OFFSET (DATA_BLOCKS_OFFSET + MAX_DATA_BLOCKS_SCALED_DOWN)

typedef struct {
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;

    uint64_t length_free_block_list;
    uint64_t number_free_blocks;
} superblock_t;

typedef struct {
    unsigned int mode;
    unsigned int taken;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int data_ptrs[NUM_DIRECT_POINTERS];
    unsigned int indirect;
} inode_t;

typedef struct {
    char names[MAX_FILENAME];
    unsigned int mode;
} directory_t;

typedef struct {
    int64_t inode;
    uint64_t rwptr;
} file_descriptor;

typedef struct {
    unsigned char num;
} bitmap_entry_t;

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
