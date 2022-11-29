#include "sfs_api.h"

unsigned int num_files = 0;
unsigned int curr_file = 0;

superblock_t super;
inode_t inodes[NUM_INODES];
file_descriptor fdt[NUM_INODES];
directory_t root[NUM_FILE_INODES];
bitmap_entry_t free_blocks[NUM_BITMAP_ENTRIES];

void init_super()
{
    super.magic = 0xACBD0005;
    super.block_size = BLOCK_SIZE;
    super.inode_table_len = NUM_INODE_BLOCKS;

    super.length_free_block_list = MAX_DATA_BLOCKS_SCALED_DOWN;
    super.number_free_blocks = (sizeof(bitmap_entry_t) * NUM_BITMAP_ENTRIES) / BLOCK_SIZE + 1;
    super.fs_size = (1 + NUM_DATA_BLOCKS_FOR_DIR + NUM_INODE_BLOCKS + MAX_DATA_BLOCKS_SCALED_DOWN + super.number_free_blocks) * BLOCK_SIZE;
    
    // printf("%lu\n", NUM_DATA_BLOCKS_FOR_DIR);
    // printf("%lu\n", NUM_INODE_BLOCKS);
    // printf("%lu\n", MAX_DATA_BLOCKS_SCALED_DOWN);
    // printf("%lu\n", NUM_BITMAP_ENTRIES);
    // printf("%lu\n", super.number_free_blocks);
    // printf("%lu\n", super.fs_size);

    super.root_dir_inode = 0;
}

void mksfs(int fresh) {
    if (fresh) {
        init_super();

        for (int i=1; i<NUM_INODES; i++) {
            inodes[i].taken = 0;
            fdt[i].inode = -1;
            memset(root[i-1].names, 0, MAX_FILENAME);
            root[i-1].mode = 0;
        }

        num_files = 0;
        curr_file = 0;
        fdt[0].inode = 0;
        fdt[0].rwptr = 0;
        inodes[0].taken = 1;
        memset(free_blocks, 0, sizeof(free_blocks));

        init_fresh_disk(DISK_NAME, BLOCK_SIZE, super.fs_size / BLOCK_SIZE + 1);
        write_blocks(0, 1, &super);
        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        write_blocks(BITMAP_BLOCK_OFFSET, super.number_free_blocks, free_blocks);

    } else {

        read_blocks(0, 1, &super);
        read_blocks(1, NUM_INODE_BLOCKS, inodes);
        read_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        read_blocks(BITMAP_BLOCK_OFFSET, super.number_free_blocks, free_blocks);

        curr_file = 0;
        num_files = 0;

        for (int i=1; i<NUM_INODES; i++) {
            if (inodes[i].taken) num_files += 1;
            fdt[i].inode = -1;
            fdt[i].rwptr = 0;
        }
    }
}

int get_free_bitmap_address() {
    int bitmap_entry = -1;
    for (int i=0; i<NUM_BITMAP_ENTRIES; i++) {
        if (free_blocks[i].num == 0) {
            bitmap_entry = i;
            break;
        }
    }
    return bitmap_entry;
}

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

int sfs_getfilesize(const char* path) {
    int size = -1;

    for (int i=0; i<NUM_FILE_INODES; i++) {
        if (strcmp(path, root[i].names) == 0) {
            /*
                we assume that size is always up-to-date and reflects size of all data blocks 
                belonging to a given file. Otherwise, we would need to read disk here to find
                the size of all data blocks that this inode points to.
            */
            size = inodes[i+1].size;
        }
    }

    return size;
}

int sfs_fopen(char* name) {
    size_t length = strlen(name);
    if (length >= MAX_FILENAME) return -1;

    for (int i=0; i<NUM_FILE_INODES; i++) {
        if (strcmp(name, root[i].names) == 0) {
            int free_fd = -1;

            for (int j=1; j<NUM_INODES; j++) {
                file_descriptor* f = &fdt[j];
                if (f->inode == i+1) return -1;
                if (free_fd == -1 && f->inode == -1) free_fd = j;
            }

            if (free_fd == -1) return -1;
            
            fdt[free_fd].inode = i+1;
            fdt[free_fd].rwptr = sfs_getfilesize(name); // sets pointer after last byte of data
            root[i].mode = 1;
            inodes[i+1].taken = 1;
            return free_fd;
        }
    }

    for (int i=1; i<NUM_INODES; i++) {
        if (inodes[i].taken == 0) {
            for (int j=1; j<NUM_INODES; j++) {
                file_descriptor* f = &fdt[j];
                if (f->inode == -1) {
                    f->inode = i;
                    f->rwptr = 0;

                    num_files += 1;
                    inodes[i].taken = 1;
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

int sfs_fclose(int fileID) {
    if (fileID > 0 && fileID < NUM_INODES) {
        file_descriptor* f = &fdt[fileID];
        if (f->inode != -1) {
            f->inode = -1;
            f->rwptr = 0;
            return 0;
        }
    }
    return -1;
}

int sfs_fwrite(int fileID, const char* buf, int length) {
    int bytes_written = 0;
    int bytes_to_write = length;
    file_descriptor* f = &fdt[fileID];

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

    // printf("File size before write: %d\n", inodes[f->inode].size);

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
            if (node->data_ptrs[current_block] > 0) {
                read_blocks(node->data_ptrs[current_block], 1, (void*) buff);
                bitmap_entry = node->data_ptrs[current_block] - DATA_BLOCKS_OFFSET;
            } else {
                if ((bitmap_entry = get_free_bitmap_address()) == -1) {
                    printf("Fatal error could not allocate empty data block.\n");
                    break;
                }
                free_blocks[bitmap_entry].num = 1;
                node->data_ptrs[current_block] = bitmap_entry + DATA_BLOCKS_OFFSET;
            }
        } else {
            if (node->indirect <= 0) {
                int ptr_bitmap_entry;
                if ((ptr_bitmap_entry = get_free_bitmap_address()) == -1) {
                    printf("Fatal error could not allocate empty data block.\n");
                    break;
                }
                free_blocks[ptr_bitmap_entry].num = 1;
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
                free_blocks[bitmap_entry].num = 1;
                ptr_buff[ptr_address] = bitmap_entry + DATA_BLOCKS_OFFSET;
            }
        }

        int block_offset = f->rwptr % BLOCK_SIZE;
        int bytes_count = BLOCK_SIZE - block_offset;
        if (bytes_to_write <= bytes_count) bytes_count = bytes_to_write;

        if (bytes_count > 0) {
            memcpy(buff+block_offset, buf+bytes_written, bytes_count);
            write_blocks(bitmap_entry + DATA_BLOCKS_OFFSET, 1, (void*) buff);

            free_blocks[bitmap_entry].num = 1;
            rwptr_size_offset += bytes_count;
            f->rwptr += bytes_count;
            bytes_to_write -= bytes_count;
            bytes_written += bytes_count;
            did_write_to_disk = 1;

            current_block = f->rwptr / BLOCK_SIZE;
        }
    }

    if (bytes_to_write != length) {
        
        // printf("R/W ptr size offset after write: %d\n", rwptr_size_offset);

        // we did write to data blocks, so we must update file metadata
        if (rwptr_size_offset > 0) node->size += rwptr_size_offset;
        if (did_load_ptr_buff) write_blocks(node->indirect, 1, (void*) ptr_buff);

        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(BITMAP_BLOCK_OFFSET, super.number_free_blocks, free_blocks);
    }

    // printf("File size after write: %d\n", inodes[f->inode].size);

    return bytes_written;
}

int sfs_fread(int fileID, char* buf, int length) {
    int bytes_read = 0;
    int bytes_to_read = length;
    file_descriptor* f = &fdt[fileID];
    
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
        // printf("R/W pointer before reading from block %d: %lu\n", current_block, f->rwptr);

        did_write_to_buf = 0;
        did_read_current_block = 0;

        char buff[BLOCK_SIZE] = "";

        if (current_block < NUM_DIRECT_POINTERS) {
            if (node->data_ptrs[current_block] > 0) {
                read_blocks(node->data_ptrs[current_block], 1, (void*) buff);
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

        // printf("R/W pointer after reading from block %d: %lu\n", current_block, f->rwptr);
    }

    return bytes_read;
}

int sfs_fseek(int fileID, int loc) {
    if (fileID > 0 && fileID < NUM_INODES) {
        file_descriptor* f = &fdt[fileID];
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

    if (inode > 0 && inodes[inode].taken == 1) {

        inode_t* n = &inodes[inode];
        char buff[BLOCK_SIZE] = "";
        unsigned int ptr_buff[NUM_POINTERS_IN_INDIRECT - 1];

        for (int i=0; i<NUM_DIRECT_POINTERS; i++) {
            if (n->data_ptrs[i] > 0) {
                int bitmap_entry = n->data_ptrs[i] - DATA_BLOCKS_OFFSET;

                // printf("Removing data block %d with bitmap index: %d\n", n->data_ptrs[i], bitmap_entry);

                free_blocks[bitmap_entry].num = 0;
                write_blocks(n->data_ptrs[i], 1, (void*) buff);
            }

            n->data_ptrs[i] = 0;
        }

        if (n->indirect > 0) {
            read_blocks(n->indirect, 1, (void*) ptr_buff);

            for (int i=0; i<NUM_POINTERS_IN_INDIRECT-1; i++) {
                if (ptr_buff[i] > 0) {
                    int bitmap_entry = ptr_buff[i] - DATA_BLOCKS_OFFSET;

                    // printf("Removing data block %d with bitmap index: %d\n", ptr_buff[i], bitmap_entry);

                    free_blocks[bitmap_entry].num = 0;
                    write_blocks(ptr_buff[i], 1, (void*) buff);
                }
            }

            write_blocks(n->indirect, 1, (void*) buff);
            n->indirect = 0;
        }

        n->mode = 0;
        n->size = 0;
        n->taken = 0;
        num_files -= 1;

        write_blocks(1, NUM_INODE_BLOCKS, inodes);
        write_blocks(1 + NUM_INODE_BLOCKS, NUM_DATA_BLOCKS_FOR_DIR, root);
        write_blocks(BITMAP_BLOCK_OFFSET, super.number_free_blocks, free_blocks);
    }

    return inode;
}
