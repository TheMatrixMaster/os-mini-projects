# Mountable File System

The purpose of this project was to build a simple mountable file system on an emulated disk that would support basic CRUD operations on files in a single root directory.

## Test Results
I have slightly modified the Makefile to output all object files in the `obj_files` directory and all executable files in the `exec_files` directory. These are the results of the tests that I have run with the test files provided for the assignment.

| Test filename     | Result    | Details                                                                           |
| ----------------- | --------- | --------------------------------------------------------------------------------- |
| sfs_test0.c       | Passed    | Successfully wrote and read to disk and printed correct string                    |
| sfs_test1.c       | Passed    | Successfully created 10 files and repeatedly wrote 267 iterations to same file    |
| sfs_test2.c       | Passed    | Successfully created 100 files and repeatedly wrote 267 iterations to same file   |
| fuse_wrap_new.c   | Passed    | Was able to mount disk onto a folder and create / edit files inside               |
| fuse_wrap_old.c   | Passed    | Was able to kill disk process and remount folder to recover all previous files    |

Please feel free to contact me at stephen.lu@mail.mcgill.ca if you are unable to replicate these test results. 

## Architecture

### Disk Format
I chose to format my disk with a block size of 1024 bytes per block. Then I partitioned the disk into the following major sections (from left to right).

| Category          | Count             | Size (in blocks)   |
| ----------------- | ----------------- | ------------------ |
| Superblock        | 1                 | 1                  |
| i-Nodes           | 128               | 9                  |
| Directory entries | 127               | 8                  |
| Data blocks       | 2135              | 2135               |
| Bitmap entries    | 2135              | 3                  |

Thus, my disk has a total size of 2156 blocks which yields 2,207,744 bytes.

### Data Structures
Here is a brief overview of the data structures used to implement my file system. For a more detailed rundown, please take a look at the `sfs_api.h` file.

- The Superblock is implemented exactly as recommended in the assignment instructions. It contains 5 metadata fields about the disk and occupies 20 bytes of data, so it will always fit in a single block.

- The i-node data structure contains metadata about its corresponding file (mode, link count, size) as well as 12 direct pointers and 1 indirect pointer. The direct pointers are implemented as an array of 12 unsigned integers and the indirect pointer is simply 1 single unsigned integer. These integers correspond to the address of their allocated data blocks on the disk.

- Each directory entry contains a char array to hold the filename and an unsigned integer to hold the file mode. The char array can hold a maximum of 60 chars and the mode is simply a duplicate field of the file mode saved in the i-node data structure. We make this duplication to facilitate our access to the mode value.

- I decided against implementing a data structure in memory to save data blocks since it was simpler for me read and write to the disk directly when interacting with file data. This does increase the overall amount of disk calls, but given the use case of my simple file system, I did not see the benefit of implementing a data block cache. This could be an interesting feature to add.

- The bitmap entries are used to keep track of free data blocks, so that these can be readily allocated when the client wants to write new data to the disk. I decided to implement my bitmap as a char vector, where each char is mapped to a data block and represents its availability. A value of 0 indicates that the data block is unused, while a value of 1 means that it is taken. Looking back, I should have probably inverted this numbering since that's proper way of implementing the bitmap, and I could have also reduced the amount of space occupied by the bitmap by using a bit-masking approach where each block would be represented by a single bit. 

- Finally, I also hold an array of file descriptor structures in memory to represent the files that the client has opened in the current session. A file descriptor struct contains an integer that points to the index of the inode data structure for the current file as well as an unsigned 64 bit integer to hold the position of the read-write pointer in the file. The file descriptor array is never saved onto the disk, so it is reset whenever a client opens a session.

### API details
Here is a high-level overview of the runtime behaviour of my filesystem API. For a more detailed description, please visit the `sfs_api.c` source code file.

- `mksfs(int fresh)` initializes the disk either as a fresh file system or by loading the data from an existing disk file. If we are making a fresh fs, then I first initialize the following data structures: superblock, inodes, directory table, bitmap array, and write them to the disk in the right positions. If I am loading an existing disk file, then I simply do the reverse: read the raw data from the disk since I know their starting addresses and load them into the corresponding in-memory data structures.

- To implement `sfs_getnextfilename(char *name)`, I have two global variables `num_files` and `curr_file` that track the total number of files in the root directory and the current file respectively. The `num_files` value is set when I initially load the fs, and I update it in the `sfs_fopen` and `sfs_fremove` methods. To get the next filename, I simply iterate through my root directory table until I reach the file indexed by `curr_file`. Then I copy the filename into `*name` and increment the `curr_file` count.

- `sfs_getfilesize(const char* path)` is the simplest method to implement. I simply loop through the directory until I find a match on the filename, and return the `size` field on the corresponding i-node data structure. As long as I properly update this `size` field, then I can always expect it to represent the exact byte size of the current file.

- `sfs_open(char *name)` first checks if the given filename is already created in the directory table. If it is, then I simply populate the file descriptor table with the proper data for the current file and set the `mode` field to `1` on both the inode and root directory structures. If the given filename does not exist, then I find an empty i-node and an empty directory entry that I initiate to hold this new file. I also create an entry in the file descriptor to indicate that this file has been opened and I increment the `num_files` global variable. Finally, I write all these modified structures to the disk.

- `sfs_close(int fileID)` checks if the fileID is pointing to a valid file descriptor in the file descriptor table. If it is, then we simply clean up this file descriptor entry. No need to modify other data structures or write to disk, since the file descriptor table lives purely in memory.

- `sfs_fwrite(int fileID, const char* buf, int length)` first uses the read-write pointer position to determine the starting block and starting position where it should write the contents of the buffer. It then uses a while loop to gradually write the data into the current block and switch blocks when we reach the end of the current block. Depending on if we are overwriting existing file data or extending the existing file, the method will appropriately allocate new data blocks using a helper method `get_free_bitmap_address()` that scans through the bitmap array to find unused data blocks. The method will also help us allocate an intermediate data block for the indirect pointer that we can subsequently fill with direct pointers to point to data blocks. Finally, `sfs_fwrite` will also appropriately update all metadata fields in the i-node (size, pointers) and move the read-write pointer in the file descriptor to the appropriate position. 

- `sfs_fread(int fileID, char* buf, int length)` uses the same ideas presented in the `sfs_fwrite` method to read a given amount of bytes from a file. Once again, we start from the read-write pointer and use the while loop to increment through the relevant data blocks whose contents we `memcpy` into the input buffer. We need to make sure that we stop reading data if we hit the end of the file contents, and this is made possible by using the `size` field.

- `sfs_fseek(int fileID, int loc)` simply grabs the file descriptor associated with the provided `fileID` and updates the read-write pointer to `loc`. We do need to make sure that `loc` is greater than 0 and less than the `size` of the file.

- `sfs_remove(char* file)` first cleans up the in-memory data structures associated with the file (file descriptor, directory entry, i-node). Before cleaning up the i-inode, the method loops through all the non-zero data pointers (direct and indirect) and deallocates the corresponding data blocks on the disk by clearing the data and setting the mapped char in the free bitmap array back to 0. Finally, we flush all changes to the disk and decrement the `num_files` global variable.
