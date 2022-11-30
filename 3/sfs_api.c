/** @file sfs_api.c
 *  @brief A simple mountable file system
 *
 *  The purpose of this project was to build a simple mountable 
 *  file system on an emulated disk that would support basic CRUD 
 *  operations on files in a single root directory.
 *
 *  @author Stephen Z. Lu (thematrixmaster)
 *  @bug No known bugs.
 */

#include "sfs_api.h"

/*
 *  num_files and curr_file are global variables that keep track of the total 
 *  number of files in the disk and the current file position, so that we can
 *  appropriately loop through the root directory in sfs_getnextfilename()
*/
unsigned int num_files = 0;
unsigned int curr_file = 0;

/* 
 *  Here, I am simply declaring my in-memory data 
 *  structures as global variables on the heap
*/
superblock_t super;
inode_t inodes[NUM_INODES];
file_descriptor_t fdt[NUM_INODES];
directory_entry_t root[NUM_FILE_INODES];
bitmap_entry_t free_blocks[MAX_DATA_BLOCKS_SCALED_DOWN];

/** @brief Helper function for initializing Superblock
 * 
 *  init_super() is a helper function that initializes the metadata fields
 *  of the superblock structure when the client makes a fresh file system
 * 
 *  @return void
*/
void init_super()
{
    super.magic = 0xACBD0005;
    super.block_size = BLOCK_SIZE;
    super.inode_table_len = NUM_INODE_BLOCKS;
    super.root_dir_inode = 0;
    super.fs_size = BLOCK_SIZE * NUM_TOTAL_BLOCKS;
}

/** @brief Helper function for finding free data blocks
 * 
 *  get_free_bitmap_address scans through the bitmap vector 
 *  to find the address of a free data block. It returns -1 
 *  if it cannot find a free data block.
 * 
 *  @return index of the free position in bitmap array
*/
int get_free_bitmap_address() {
    int bitmap_entry = -1;
    for (int i=0; i<MAX_DATA_BLOCKS_SCALED_DOWN; i++) {
        if (free_blocks[i] == 0) {
            bitmap_entry = i;
            break;
        }
    }
    return bitmap_entry;
}

/** @brief Initializes the file system
 * 
 *  `mksfs(int fresh)` initializes the disk either as a fresh file system 
 *  or by loading the data from an existing disk file. If we are making a 
 *  fresh fs, then I first initialize the following data structures: 
 *  superblock, inodes, directory table, bitmap array, and write them to 
 *  the disk in the right positions. If I am loading an existing disk file, 
 *  then I simply do the reverse: read the raw data from the disk since I 
 *  know their starting addresses and load them into the corresponding 
 *  in-memory data structures.
 * 
 *  @param fresh to initialize disk from scratch or load from file
 *  @return Void
*/
void mksfs(int fresh) {
    if (fresh) {
        init_super();

        for (int i=1; i<NUM_INODES; i++) {
            inodes[i].link_cnt = 0;
            fdt[i].inode = -1;
            memset(root[i-1].names, 0, MAX_FILENAME);
            root[i-1].mode = 0;
        }

        num_files = 0;
        curr_file = 0;
        fdt[0].inode = 0;
        fdt[0].rwptr = 0;
        inodes[0].link_cnt = 1;
        memset(free_blocks, 0, sizeof(free_blocks));

        init_fresh_disk(DISK_NAME, BLOCK_SIZE, NUM_TOTAL_BLOCKS);
        write_blocks(0, 1, &super);
        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        write_blocks(BITMAP_BLOCK_OFFSET, NUM_DATA_BLOCKS_FOR_BITMAP, free_blocks);

    } else {
        init_disk(DISK_NAME, BLOCK_SIZE, NUM_TOTAL_BLOCKS);

        read_blocks(0, 1, &super);
        read_blocks(1, NUM_INODE_BLOCKS, inodes);
        read_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        read_blocks(BITMAP_BLOCK_OFFSET, NUM_DATA_BLOCKS_FOR_BITMAP, free_blocks);

        curr_file = 0;
        num_files = 0;

        for (int i=1; i<NUM_INODES; i++) {
            if (inodes[i].link_cnt) num_files += 1;
            fdt[i].inode = -1;
            fdt[i].rwptr = 0;
        }

        fdt[0].inode = 0;
        fdt[0].rwptr = 0;
    }
}

/** @brief Gets next filename in directory
 * 
 *  To implement `sfs_getnextfilename(char *name)`, I have two global variables 
 *  `num_files` and `curr_file` that track the total number of files in the root 
 *  directory and the current file respectively. The `num_files` value is set when 
 *  I initially load the fs, and I update it in the `sfs_fopen` and `sfs_fremove` 
 *  methods. To get the next filename, I simply iterate through my root directory 
 *  table until I reach the file indexed by `curr_file`. Then I copy the filename into 
 *  `*name` and increment the `curr_file` count.
 *  
 *  @param fname buffer to write the next filename
 *  @return 1 for exit success and 0 otherwise
*/
int sfs_getnextfilename(char* fname) {
    if (num_files > 0) {
        int counter = 0;

        for (int i=0; i<NUM_FILE_INODES; i++) {
            if (root[i].mode != 1) continue;
            if (counter == curr_file) {
                strcpy(fname, root[i].names);
                curr_file += 1;
                return 1;
            }
            counter += 1;
        }
    }

    curr_file = 0;
    return 0;
}

/** @brief Get the file size at given path
 * 
 *  `sfs_getfilesize(const char* path)` is the simplest method to implement. 
 *  I simply loop through the directory until I find a match on the filename, 
 *  and return the `size` field on the corresponding i-node data structure. 
 *  As long as I properly update this `size` field, then I can always expect 
 *  it to represent the exact byte size of the current file.
 * 
 *  @param path the path of the requested file
 *  @return size of file at path in bytes
*/
int sfs_getfilesize(const char* path) {
    int size = -1;

    for (int i=0; i<NUM_FILE_INODES; i++) {
        if (strcmp(path, root[i].names) == 0) {
            /**
             * we assume that size is always up-to-date and reflects size of all data blocks 
             * belonging to a given file. Otherwise, we would need to read disk here to find
             * the size of all data blocks that this inode points to.
            */
            size = inodes[i+1].size;
        }
    }

    return size;
}

/** @brief Open a file in append mode 
 * 
 *  `sfs_open(char *name)` first checks if the given filename is already created 
 *  in the directory table. If it is, then I simply populate the file descriptor 
 *  table with the proper data for the current file and set the `mode` field to `1` 
 *  on both the inode and root directory structures. If the given filename does not 
 *  exist, then I find an empty i-node and an empty directory entry that I initiate 
 *  to hold this new file. I also create an entry in the file descriptor to indicate 
 *  that this file has been opened and I increment the `num_files` global variable. 
 *  Finally, I write all these modified structures to the disk.
 * 
 *  @param name the name of the file to open
 *  @return file descriptor of file on success and -1 on failure
*/
int sfs_fopen(char* name) {
    size_t length = strlen(name);
    if (length >= MAX_FILENAME) return -1;

    for (int i=0; i<NUM_FILE_INODES; i++) {
        if (strcmp(name, root[i].names) == 0) {
            int free_fd = -1;

            for (int j=1; j<NUM_INODES; j++) {
                file_descriptor_t* f = &fdt[j];
                if (f->inode == i+1) return -1;
                if (free_fd == -1 && f->inode == -1) free_fd = j;
            }

            if (free_fd == -1) return -1;
            
            fdt[free_fd].inode = i+1;
            fdt[free_fd].rwptr = sfs_getfilesize(name); // sets pointer after last byte of data
            root[i].mode = 1;
            inodes[i+1].link_cnt = 1;
            return free_fd;
        }
    }

    for (int i=1; i<NUM_INODES; i++) {
        if (inodes[i].link_cnt == 0) {
            for (int j=1; j<NUM_INODES; j++) {
                file_descriptor_t* f = &fdt[j];
                if (f->inode == -1) {
                    f->inode = i;
                    f->rwptr = 0;

                    num_files += 1;
                    inodes[i].link_cnt = 1;
                    inodes[i].mode = 1;
                    inodes[i].size = 0;

                    strcpy(root[i-1].names, name);
                    root[i-1].mode = 1;

                    write_blocks(1, NUM_INODE_BLOCKS, inodes);
                    write_blocks(1+NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);

                    return j;
                }
            }
        }
    }

    return -1;
}

/** @brief Close a file
 * 
 *  `sfs_close(int fileID)` checks if the fileID is pointing to a valid file 
 *  descriptor in the file descriptor table. If it is, then we simply clean up 
 *  this file descriptor entry. No need to modify other data structures or write 
 *  to disk, since the file descriptor table lives purely in memory.
 * 
 *  @param fileID the file descriptor of the file to close
 *  @return 0 on success and -1 on failure
*/
int sfs_fclose(int fileID) {
    if (fileID > 0 && fileID < NUM_INODES) {
        file_descriptor_t* f = &fdt[fileID];
        if (f->inode != -1) {
            f->inode = -1;
            f->rwptr = 0;
            return 0;
        }
    }
    return -1;
}

/** @brief Write contents of buffer to file
 * 
 *  `sfs_fwrite(int fileID, const char* buf, int length)` first uses the 
 *  read-write pointer position to determine the starting block and starting 
 *  position where it should write the contents of the buffer. It then uses a 
 *  while loop to gradually write the data into the current block and switch 
 *  blocks when we reach the end of the current block. Depending on if we are 
 *  overwriting existing file data or extending the existing file, the method 
 *  will appropriately allocate new data blocks using a helper method 
 *  `get_free_bitmap_address()` that scans through the bitmap array to find 
 *  unused data blocks. The method will also help us allocate an intermediate 
 *  data block for the indirect pointer that we can subsequently fill with direct 
 *  pointers to point to data blocks. Finally, `sfs_fwrite` will also appropriately 
 *  update all metadata fields in the i-node (size, pointers) and move the 
 *  read-write pointer in the file descriptor to the appropriate position. 
 * 
 *  @param fileID the file descriptor of the file to write to
 *  @param buf the buffer that holds the content we wish to write to disk
 *  @param length the amount of bytes in the buffer to write to disk
 *  @return the number of bytes written to disk
*/
int sfs_fwrite(int fileID, const char* buf, int length) {
    int bytes_written = 0;
    int bytes_to_write = length;
    file_descriptor_t* f = &fdt[fileID];

    if (
        length <= 0 ||
        f->inode <= 0 ||
        f->rwptr < 0 ||
        f->rwptr > inodes[f->inode].size || // can't skip over empty bytes in data block
        f->rwptr >= (MAX_DATA_BLOCKS_PER_FILE * BLOCK_SIZE)
    ) return 0;

    int bitmap_entry;
    int did_write_to_disk = 1;
    int current_block = f->rwptr / BLOCK_SIZE;
    int rwptr_size_offset = -(inodes[f->inode].size - f->rwptr);

    inode_t* node = &inodes[f->inode];

    int did_load_ptr_buff = 0;
    unsigned int ptr_buff[NUM_POINTERS_IN_INDIRECT - 1];

    if (node->indirect > 0 && !did_load_ptr_buff) {
        read_blocks(node->indirect, 1, (void*) ptr_buff);
        did_load_ptr_buff = 1;
    }

    while (
        did_write_to_disk &&
        bytes_to_write > 0 &&
        current_block < (MAX_DATA_BLOCKS_PER_FILE - 1)
    ) {
        bitmap_entry = -1;
        did_write_to_disk = 0;

        char buff[BLOCK_SIZE] = "";
        
        if (current_block < NUM_DIRECT_POINTERS) {
            if (node->direct[current_block] > 0) {
                read_blocks(node->direct[current_block], 1, (void*) buff);
                bitmap_entry = node->direct[current_block] - DATA_BLOCKS_OFFSET;
            } else {
                if ((bitmap_entry = get_free_bitmap_address()) == -1) {
                    printf("Fatal error could not allocate empty data block.\n");
                    break;
                }
                free_blocks[bitmap_entry] = 1;
                node->direct[current_block] = bitmap_entry + DATA_BLOCKS_OFFSET;
            }
        } else {
            if (node->indirect <= 0) {
                int ptr_bitmap_entry;
                if ((ptr_bitmap_entry = get_free_bitmap_address()) == -1) {
                    printf("Fatal error could not allocate empty data block.\n");
                    break;
                }
                free_blocks[ptr_bitmap_entry] = 1;
                memset(ptr_buff, 0, sizeof(ptr_buff));

                did_load_ptr_buff = 1;
                node->indirect = ptr_bitmap_entry + DATA_BLOCKS_OFFSET;
            }

            if (did_load_ptr_buff == 0) {
                printf("Fatal error could not load indirect pointer buffer\n");
                break;
            }

            int ptr_address = current_block-NUM_DIRECT_POINTERS;
            if (ptr_buff[ptr_address] > 0) {
                read_blocks(ptr_buff[ptr_address], 1, (void*) buff);
                bitmap_entry = ptr_buff[ptr_address] - DATA_BLOCKS_OFFSET;
            } else {
                if ((bitmap_entry = get_free_bitmap_address()) == -1) {
                    printf("Fatal error could not allocate empty data block.\n");
                    break;
                }
                free_blocks[bitmap_entry] = 1;
                ptr_buff[ptr_address] = bitmap_entry + DATA_BLOCKS_OFFSET;
            }
        }

        int block_offset = f->rwptr % BLOCK_SIZE;
        int bytes_count = BLOCK_SIZE - block_offset;
        if (bytes_to_write <= bytes_count) bytes_count = bytes_to_write;

        if (bytes_count > 0) {
            memcpy(buff+block_offset, buf+bytes_written, bytes_count);
            write_blocks(bitmap_entry + DATA_BLOCKS_OFFSET, 1, (void*) buff);

            free_blocks[bitmap_entry] = 1;
            rwptr_size_offset += bytes_count;
            f->rwptr += bytes_count;
            bytes_to_write -= bytes_count;
            bytes_written += bytes_count;
            did_write_to_disk = 1;

            current_block = f->rwptr / BLOCK_SIZE;
        }
    }

    if (bytes_to_write != length) {
        // we did write to data blocks, so we must update file metadata
        if (rwptr_size_offset > 0) node->size += rwptr_size_offset;
        if (did_load_ptr_buff) write_blocks(node->indirect, 1, (void*) ptr_buff);

        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(BITMAP_BLOCK_OFFSET, NUM_DATA_BLOCKS_FOR_BITMAP, free_blocks);
    }

    return bytes_written;
}


/** @brief Read data from file
 * 
 *  `sfs_fread(int fileID, char* buf, int length)` uses the same ideas presented 
 *  in the `sfs_fwrite` method to read a given amount of bytes from a file. 
 *  Once again, we start from the read-write pointer and use the while loop 
 *  to increment through the relevant data blocks whose contents we `memcpy` 
 *  into the input buffer. We need to make sure that we stop reading data if 
 *  we hit the end of the file contents, and this is made possible by using the 
 *  `size` field.
 * 
 *  @param fileID file descriptor of the file to read from
 *  @param buf char buffer to read data into
 *  @param length amount of data to read in bytes
 *  @return the actual of data read in bytes
*/
int sfs_fread(int fileID, char* buf, int length) {
    int bytes_read = 0;
    int bytes_to_read = length;
    file_descriptor_t* f = &fdt[fileID];
    
    if (
        length <= 0 ||
        f->inode <= 0 ||
        f->rwptr < 0 ||
        f->rwptr >= inodes[f->inode].size   // can't read after last byte of data
    ) return 0;
    
    int did_write_to_buf = 1;
    int did_read_current_block;
    int current_block = f->rwptr / BLOCK_SIZE;

    int rwptr_size_offset = inodes[f->inode].size - f->rwptr;
    if (rwptr_size_offset < bytes_to_read) bytes_to_read = rwptr_size_offset;

    inode_t* node = &inodes[f->inode];

    int did_load_ptr_buff = 0;
    unsigned int ptr_buff[NUM_POINTERS_IN_INDIRECT - 1];

    while (
        did_write_to_buf &&
        bytes_to_read > 0 &&
        current_block < (MAX_DATA_BLOCKS_PER_FILE - 1)
    ) {
        did_write_to_buf = 0;
        did_read_current_block = 0;

        char buff[BLOCK_SIZE] = "";

        if (current_block < NUM_DIRECT_POINTERS) {
            if (node->direct[current_block] > 0) {
                read_blocks(node->direct[current_block], 1, (void*) buff);
                did_read_current_block = 1;
            }
        } else {
            if (!did_load_ptr_buff && node->indirect > 0) {
                read_blocks(node->indirect, 1, (void*) ptr_buff);
                did_load_ptr_buff = 1;
            }

            int ptr_address = current_block-NUM_DIRECT_POINTERS;
            if (did_load_ptr_buff && ptr_buff[ptr_address] > 0) {
                read_blocks(ptr_buff[ptr_address], 1, (void*) buff);
                did_read_current_block = 1;
            }
        }

        if (did_read_current_block) {
            int block_offset = f->rwptr % BLOCK_SIZE;
            int bytes_count = BLOCK_SIZE - block_offset;

            if (bytes_to_read <= bytes_count) bytes_count = bytes_to_read;
            
            if (bytes_count > 0) {
                memcpy(buf + bytes_read, buff + block_offset, bytes_count);

                did_write_to_buf = 1;
                bytes_read += bytes_count;
                bytes_to_read -= bytes_count;
                f->rwptr += bytes_count;

                current_block = f->rwptr / BLOCK_SIZE;
            }
        }
    }

    return bytes_read;
}

/** @brief Move a file's read write pointer
 * 
 *  `sfs_fseek(int fileID, int loc)` simply grabs the file descriptor 
 *  associated with the provided `fileID` and updates the read-write 
 *  pointer to `loc`. We do need to make sure that `loc` is greater 
 *  than 0 and less than the `size` of the file.
 * 
 *  @param fileID the file descriptor of the file to manipulate
 *  @param loc the new read-write pointer address
 *  @return 0 on success and -1 on failure
*/
int sfs_fseek(int fileID, int loc) {
    if (fileID > 0 && fileID < NUM_INODES) {
        file_descriptor_t* f = &fdt[fileID];
        if (
            f->inode == -1 ||
            f->inode == 0 ||
            loc < 0 ||
            loc > inodes[f->inode].size ||
            loc >= (MAX_DATA_BLOCKS_PER_FILE * BLOCK_SIZE)
        ) {
            return -1;
        }

        f->rwptr = loc;
        return 0;
    }

    return -1;
}

/** @brief Close a file and remove it from the file system 
 * 
 *  `sfs_remove(char* file)` first cleans up the in-memory data structures 
 *  associated with the file (file descriptor, directory entry, i-node). 
 *  Before cleaning up the i-inode, the method loops through all the non-zero 
 *  data pointers (direct and indirect) and deallocates the corresponding 
 *  data blocks on the disk by clearing the data and setting the mapped char 
 *  in the free bitmap array back to 0. Finally, we flush all changes to the 
 *  disk and decrement the `num_files` global variable.
 * 
 *  @param file the filename of file to remove
 *  @return the inode number of the removed file on success and -1 otherwise
*/
int sfs_remove(char* file) {

    int inode = -1;

    for (int i=0; i<NUM_FILE_INODES; i++) {
        if (strcmp(root[i].names, file) == 0) {
            inode = i + 1;
            root[i].mode = 0;
            memset(root[i].names, 0, MAX_FILENAME);

            for (int j=1; j<NUM_INODES; j++) {
                if (fdt[j].inode == inode) {
                    sfs_fclose(j);
                    break;
                }
            }
        }
    }

    if (inode > 0 && inodes[inode].link_cnt == 1) {

        inode_t* n = &inodes[inode];
        char buff[BLOCK_SIZE] = "";
        unsigned int ptr_buff[NUM_POINTERS_IN_INDIRECT - 1];

        for (int i=0; i<NUM_DIRECT_POINTERS; i++) {
            if (n->direct[i] > 0) {
                int bitmap_entry = n->direct[i] - DATA_BLOCKS_OFFSET;
                free_blocks[bitmap_entry] = 0;
                write_blocks(n->direct[i], 1, (void*) buff);
            }

            n->direct[i] = 0;
        }

        if (n->indirect > 0) {
            read_blocks(n->indirect, 1, (void*) ptr_buff);

            for (int i=0; i<NUM_POINTERS_IN_INDIRECT-1; i++) {
                if (ptr_buff[i] > 0) {
                    int bitmap_entry = ptr_buff[i] - DATA_BLOCKS_OFFSET;
                    free_blocks[bitmap_entry] = 0;
                    write_blocks(ptr_buff[i], 1, (void*) buff);
                }
            }

            write_blocks(n->indirect, 1, (void*) buff);
            n->indirect = 0;
        }

        n->mode = 0;
        n->size = 0;
        n->link_cnt = 0;
        num_files -= 1;

        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        write_blocks(BITMAP_BLOCK_OFFSET, NUM_DATA_BLOCKS_FOR_BITMAP, free_blocks);
    }

    return inode;
}
