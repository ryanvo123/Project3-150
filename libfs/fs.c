#include "fs.h"
#include "disk.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS_FILENAME_LEN 16
#define FS_FILE_MAX_COUNT 128
#define FS_OPEN_MAX_COUNT 32
#define FAT_EOC 0xFFFF

typedef struct  FileDescriptor {
  char filename[FS_FILENAME_LEN]; 
  size_t offset;                  
  int open;                     
} FileDescriptor;

static FileDescriptor file_descriptors[FS_FILE_MAX_COUNT];

typedef struct __attribute__((__packed__)) superB {
  uint8_t sig[8];
  uint16_t block_count;
  uint16_t root_block_index;
  uint16_t data_block_index;
  uint16_t data_block_count;
  uint8_t fat_block_count;
  uint8_t padding[4079];
} superB_s;

typedef struct __attribute__((__packed__)) fat {
  uint16_t entries[2048];
} fat_s;

typedef struct __attribute__((__packed__)) root_directory {
  char file_name[16];
  uint32_t file_size;
  uint16_t file_first_block;
  uint8_t padding[10];
} root_directory_s;

struct superB superB;
struct fat *fat;
struct root_directory root_directory[128];
struct FileDescriptor file_descriptor[FS_FILE_MAX_COUNT];

int fs_mount(const char *diskname) {

  if (block_disk_open(diskname) != 0) {
    return -1;
  }
  // Read the superblock from the disk
  superB_s superblock;
  if (block_read(0, &superblock) != 0) {
    block_disk_close();
    return -1;
  }
  
  fat = malloc(superblock.fat_block_count * BLOCK_SIZE);
  if (fat == NULL) {
    block_disk_close(); 
    return -1;
  }

  for(int i = 1; i <= superblock.fat_block_count; i++) {
    if (block_read(i, &fat->entries[i]) != 0) {
      free(fat);
      block_disk_close();
      return -1;
    }
  }
  if (block_read(superblock.root_block_index, &root_directory) != 0) {
    free(fat);
    block_disk_close();
    return -1;
  }
  
  return 0;
}

int fs_umount(void) {
  
  superB_s superblock;
  
  if(fs_mount("") != 0) {
    return -1;
  }

  if (block_write(0, &superblock) != 0) {
    return -1;
  }

  for(int i = 1; i <= superblock.fat_block_count; i++) {
    if (block_write(i, &fat->entries[i]) != 0) {
      return -1;
    }
  }
  
  if (block_write(superblock.root_block_index, &root_directory) != 0) {
    return -1;
  }
  
  if (block_disk_close() != 0) {
    return -1;
  }
  
  for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
    if (file_descriptors[i].open) {
      return -1; 
    }
  }
  
  free(fat);
  return 0;
  
}

// needs to print ratio, go into csif and run reference program info
int fs_info(void) {
  
    superB_s superblock;
    if (block_read(0, &superblock) != 0) {
      return -1;
    } else {
      printf("FS Info:\n");
      printf("Block Signature: %s\n", superblock.sig);
      printf("Block Count: %d\n", superblock.block_count);
      printf("Root Block Index: %d \n", superblock.root_block_index);
      printf("Data Block Index: %d \n", superblock.data_block_index);
      printf("Data Block Count: %d \n", superblock.data_block_count);
      printf("Fat Block Count: %d \n", superblock.fat_block_count);
    }
    // for loops, go through root direct, if filename isnt empty up the count of used root directory block
  // for the ratio
    int rootemptcount = 0;
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
      if(root_directory[i].file_name[0] == '\0') {
        rootemptcount++;
      }
    }

    int fatemptcount = 0;
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
      if(fat[i].entries[0] == 0) {
        fatemptcount++;
      }
    }
    //ratio calc
    double root_ratio = rootemptcount / FS_FILE_MAX_COUNT;
    double fat_ratio = fatemptcount / superblock.data_block_count;
    printf("Root Directory Ratio: %f\n", root_ratio);
    printf("FAT Ratio: %f\n", fat_ratio);
    return 0;
}

// go through root direct, if filename isnt in root direct, add to root direct 
int fs_create(const char *filename) {
  
  if(fs_mount("") != 0) {
    return -1;
  }
  
  // Check if a file with the same name already exists
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    // root_directory.file_name, filename, FS_FILENAME_LEN
    if (strncmp(root_directory[i].file_name, filename, FS_FILENAME_LEN) == 0) {
      // File with the same name already exists
      return -1; // Return an error code (you can choose an appropriate one)
    }
  }
  // Find an empty slot in the root directory
  int index = -1;
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (root_directory[i].file_name[0] == '\0') {
      index = i;
      break;
    }
  }
  
  if (index != -1) {
    // Initialize the new file entry
    strncpy(root_directory[index].file_name, filename, FS_FILENAME_LEN);
    root_directory[index].file_size = 0; // Initialize file size
    root_directory[index].file_first_block = FAT_EOC; // Mark as unused
    // Return the index of the newly created file
    block_write(superB.root_block_index, &root_directory);
    return index;
  } else {
    // No empty slot available
    return -1; // Return an error code
  }
  
  return 0;
}

int fs_delete(const char *filename) {
  
  int root_index = 0;
  while (root_index < FS_FILE_MAX_COUNT) {
    if (strncmp(root_directory[root_index].file_name, filename, FS_FILENAME_LEN) == 0) {
    // File found
      root_directory[root_index].file_name[0] = '\0'; // Mark as unused
      root_directory[root_index].file_size = 0; // Set file size to 0
      root_directory[root_index].file_first_block = FAT_EOC; // Unlink file blocks
      block_write(superB.root_block_index, &root_directory);
      return 0; // Success
    }
    root_index++;
  }
  // File not found
  return -1; // Error: File not found
  }


int fs_ls(void) {
  // Print the contents of the root directory
  printf("Root Directory Contents:\n");
  
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (root_directory[i].file_name[0] != '\0') {
      printf("File Name: %s, size: %d, data_blk: %d\n", root_directory[i].file_name, 
      root_directory[i].file_size, root_directory[i].file_first_block);
    }
  }
  return 0;
}

int fs_open(const char *filename) { 
  
  if(fs_mount("") != 0) {
    return -1;
  }
  
  int file_indx = -1; 
  
  for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if(strncmp(file_descriptors[i].filename, filename, FS_FILENAME_LEN) == 0 ) {
      file_indx = i;
      break;
    }
  }
  
  // error check if file is already open
  if(file_indx == -1){
    return -1;  
  }

  strncpy(file_descriptors[file_indx].filename, filename, FS_FILENAME_LEN);
  file_descriptors[file_indx].offset = 0;
  file_descriptors[file_indx].open = true;

  // Return the file descriptor index
  return file_indx;
}

int fs_close(int fd) { 
  
  if (fd < 0 || fd >= FS_FILE_MAX_COUNT) {
    return -1; // Invalid file descriptor
  }
  if (file_descriptors[fd].open == false) {
    return -1; // File is not open
  }
  file_descriptors[fd].open = false; // Close the file
  return 0;
}

int fs_stat(int fd) { 
  if (fs_mount("") != 0) {
    return -1; // No filesystem mounted
  }
  // Validate the file descriptor
  if (fd < 0 || fd >= FS_FILE_MAX_COUNT || file_descriptors[fd].open == false) {
    return -1; // Invalid file descriptor
  }
  
  uint16_t file_f_block = root_directory[fd].file_first_block;
  
    if (file_f_block == FAT_EOC || file_f_block >= superB.data_block_count) {
        // Invalid file block index
        return -1;
    }

    uint32_t file_size = root_directory[fd].file_size;
    return file_size;
}

int fs_lseek(int fd, size_t offset) { 
  
  if (fs_mount("") != 0) {
    return -1; // no filesystem mounted return error 
  }
  
  if (fd < 0 || fd >= FS_FILE_MAX_COUNT || file_descriptors[fd].open == false) {
    return -1; // error checking 
  }

  file_descriptors[fd].offset = offset;

  return 0;

}

// int fs_write(int fd, void *buf, size_t count) { 
  
//   if (fs_mount("") != 0) {
//     return -1; // no filesystem mounted return error
//   }
  
//   if (fd < 0 || fd >= FS_FILE_MAX_COUNT || file_descriptors[fd].open) {
//     return -1; // error checking
//   }
  
  
// }

// int fs_read(int fd, void *buf, size_t count) { 

//   if (fs_mount("") != 0) {
//     return -1; 
//   }

//   if (fd < 0 || fd >= FS_FILE_MAX_COUNT || file_descriptors[fd].open == false) {
//     return -1;
//   }

// }