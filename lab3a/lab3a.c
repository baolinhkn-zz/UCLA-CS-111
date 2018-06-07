#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

#define BUFF_SIZE 2048
#define superblock_offset 1024

int ext2fd; //file descriptor for disk image
char buffer[BUFF_SIZE];
unsigned int block_size;
unsigned int inode_count;
int block_count;
int blocks_bitmap;
int inodes_bitmap;
int inode_table;
char justtime[20];

void systemCallErr(char syscall[]);

void superblock_summary(void)
{
  int toRead = sizeof(struct ext2_super_block);
  toRead = pread(ext2fd, buffer, toRead, superblock_offset);
  if (toRead < 0)
    systemCallErr("pread");
  struct ext2_super_block* superblock_ptr;
  superblock_ptr = (struct ext2_super_block*) buffer;
  printf("SUPERBLOCK,");
  //block count
  block_count = superblock_ptr->s_blocks_count;
  printf("%d,", block_count);
  //inode count
  inode_count = superblock_ptr->s_inodes_count;
  printf("%d,", inode_count);
  //block size
  block_size = 1024 << (superblock_ptr->s_log_block_size);
  printf("%d,", block_size);
  //inode size
  printf("%d,", superblock_ptr->s_inode_size);
  //blocks per group
  printf("%d,", superblock_ptr->s_blocks_per_group);
  //inodes per group
  printf("%d,", superblock_ptr->s_inodes_per_group);
  //first non-reserved i-node
  printf("%d\n", superblock_ptr->s_first_ino);
}

void group_summary(void) 
{
  int toRead = sizeof(struct ext2_group_desc);
  toRead = pread(ext2fd, buffer, toRead, superblock_offset+sizeof(struct ext2_super_block));
  if (toRead < 0)
    systemCallErr("pread");
  struct ext2_group_desc* group_pointer = (struct ext2_group_desc*)buffer;
  printf("GROUP,");
  // group number
  printf("%d,", 0);
  // total number of blocks in the group
  printf("%d,", block_count);
  // total number of inodes in the group
  printf("%d,", inode_count);
  // total number of free blocks
  printf("%d,", group_pointer->bg_free_blocks_count);
  // total number of free inodes
  printf("%d,", group_pointer->bg_free_inodes_count);
  // block number of free block bitmap
  blocks_bitmap = group_pointer->bg_block_bitmap;
  printf("%d,", blocks_bitmap);
  // block number of the free inode bitmap
  inodes_bitmap = group_pointer->bg_inode_bitmap;
  printf("%d,", inodes_bitmap);
  // block number of the inode table
  inode_table = group_pointer->bg_inode_table;
  printf("%d\n", inode_table);

}

void free_blocks(void) 
{
  uint8_t* bitmap = malloc(block_size*sizeof(uint8_t));
  int toRead = pread(ext2fd, bitmap, block_size, superblock_offset+(blocks_bitmap-1)*block_size);
  if (toRead < 0)
    systemCallErr("pread");

  uint32_t i;
  uint32_t bitmap_size = block_count;
  int counter = 0;

  for (i = 0; i < bitmap_size; i++)
    {
      uint8_t a_byte = bitmap[i/8];
      if (!(a_byte & (1 << (i % 8))))
	{
	  counter++;
	  printf("BFREE,");
	  printf("%d\n", i+1);
	}
    }
  free(bitmap);
}

void free_inodes(void)
{
  uint8_t *bitmap = malloc(block_size);
  int toRead = pread(ext2fd, bitmap, block_size, superblock_offset+(inodes_bitmap-1)*block_size); 
  if (toRead < 0)
    {
      fprintf(stderr, "Error!");
      // system call error function
      systemCallErr("pread");
    }
  uint32_t bitmap_size = inode_count;
  uint32_t bit_num;
  //first index of inode table is 1
  int inode_ctr = 1;
  int free_inodes = 0;


  for (bit_num = 0; bit_num < bitmap_size; bit_num++)
    {
      uint8_t a_byte = bitmap[bit_num / 8];
      if (!(a_byte & (1 << (bit_num % 8))))
	{
	  printf("IFREE,%d\n", inode_ctr);
	  free_inodes++;
	}
      inode_ctr++;
    }  
  free(bitmap);
}

void convert_time(uint32_t seconds, char* buffer) {
  time_t rawtime = (time_t)seconds;
  struct tm* info;

  time(&rawtime);
  info = gmtime(&rawtime);
  strftime(buffer, 20, "%x %X", info);
}


void dir_data_block(char* block, struct ext2_inode* inode_ptr, int inode_num, int block_num)
{
  int toRead = pread(ext2fd, block, block_size, superblock_offset + (block_num-1) * block_size);
  if (toRead < 0)
    systemCallErr("pread");
  struct ext2_dir_entry* dir_ptr = (struct ext2_dir_entry*) block;
  unsigned int position = 0;
  while (position < inode_ptr->i_size)
    {
      if (dir_ptr->inode > 0 && dir_ptr->name_len > 0)
	{
	  printf("DIRENT,");
	  //parent inode number
	  printf("%d,", inode_num);
	  //logical byte offset, position
	  printf("%d,", position);
	  //inode number
	  printf("%d,", dir_ptr->inode);
	  //length of entry
	  printf("%d,", dir_ptr->rec_len);
	  //length of name
	  int name_len = dir_ptr->name_len;
	  printf("%d,", name_len);
	  int j;
	  char quote = '\'';
	  //name
	  printf("%c", quote);
	  for (j = 0; j < name_len; j++)
	    printf("%c", dir_ptr->name[j]);
	  printf("%c", quote);
	  printf("\n");	      
	}
      else
	break;
      position = position + dir_ptr->rec_len;
      dir_ptr = (void*)dir_ptr + dir_ptr->rec_len;
    } 
}

void indirect_block(char* block, struct ext2_inode* inode_ptr, int inode_num, int block_num, char file_type, int logical_offset)
{
  int indirect_block[BUFF_SIZE];
  int toRead = pread(ext2fd, indirect_block, block_size, superblock_offset + (block_num-1) * block_size);
  if (toRead < 0)
    systemCallErr("pread");
  unsigned int j = 0;
  while (j < block_size/4)
    {
	if (indirect_block[j] == 0)
	{
	  j++;
	  continue;
	}

      if (file_type == 'd')
	dir_data_block(block, inode_ptr, inode_num, indirect_block[j]);
      printf("INDIRECT,");
      //inode number of owning file
      printf("%d,", inode_num);
      //level of indirection
      printf("1,");
      //logical block offset
      printf("%d,", logical_offset+j);
      //block being scanned
      printf("%d,", block_num);
      //referenced block
      printf("%d\n", indirect_block[j]);
      j++;
      }
}

void double_indirect_block(char* block, struct ext2_inode* inode_ptr, int inode_num, int block_num, char file_type, int logical_offset)
{
  int double_indirect_block[BUFF_SIZE];
  int toRead = pread(ext2fd, double_indirect_block, block_size, superblock_offset + (block_num-1) * block_size);
  if (toRead < 0)
    systemCallErr("pread");
  unsigned int i;
  for (i = 0; i < block_size/4; i++)
    {
      if (double_indirect_block[i] == 0)
	continue;
      indirect_block(block, inode_ptr, inode_num, double_indirect_block[i], file_type, i+logical_offset);
      printf("INDIRECT,");
      //inode number of owning file
      printf("%d,", inode_num);
      //level of indirection
      printf("2,");
      //logical block offset
      printf("%d,", logical_offset+i);
      //block being scanned
      printf("%d,", block_num);
      //referenced block
      printf("%d\n", double_indirect_block[i]);

    }
}

void triple_indirect_block(char* block, struct ext2_inode* inode_ptr, int inode_num, int block_num, char file_type, int logical_offset)
{
  int triple_indirect_block[BUFF_SIZE];
  int toRead = pread(ext2fd, triple_indirect_block, block_size, superblock_offset + (block_num-1) * block_size);
  if (toRead < 0)
    systemCallErr("pread");
  unsigned int i;
  for (i = 0; i < block_size/4; i++)
    {
      if (triple_indirect_block[i] == 0)
	continue;
      double_indirect_block(block, inode_ptr, inode_num, triple_indirect_block[i], file_type, i+logical_offset);
      printf("INDIRECT,");
      //inode number of owning file
      printf("%d,", inode_num);
      //level of indirection
      printf("3,");
      //logical block offset
      printf("%d,", logical_offset + i);
      //block being scanned
      printf("%d,", block_num);
      //referenced block
      printf("%d\n", triple_indirect_block[i]);

    }
}

void directory_entry(struct ext2_inode* inode_ptr, int inode_num, char file_type)
{
  char block[BUFF_SIZE];
  int i;
  //direct blocks
  if (file_type == 'd')
    {
      for (i = 0; i < 12; i++)
	{
	  if (inode_ptr->i_block[i] == 0)
	    return;
	  dir_data_block(block, inode_ptr, inode_num, inode_ptr->i_block[i]);
	}
    }
  //indirect block
  if (inode_ptr->i_block[12] != 0)
    indirect_block(block, inode_ptr, inode_num, inode_ptr->i_block[12], file_type, 12);
  //double indirect block
  if (inode_ptr->i_block[13] != 0)
    double_indirect_block(block, inode_ptr, inode_num, inode_ptr->i_block[13], file_type, 12+256);
  //triple indirect block
  if (inode_ptr->i_block[14] != 0)
    triple_indirect_block(block, inode_ptr, inode_num, inode_ptr->i_block[14], file_type, 12 + (256 * 256)+256);
}

void inode_summary(void)
{  
  unsigned int i;
  char create_buffer[20];
  char modified_buffer[20];
  char accessed_buffer[20];

  for (i = 0; i < inode_count; i++)
    {
      int toRead = sizeof(struct ext2_inode);
      toRead = pread(ext2fd, buffer, toRead, (superblock_offset + (block_size * (inode_table-1)) + (i * toRead)));
      if (toRead < 0)
	systemCallErr("pread");
      struct ext2_inode* inode_ptr = (struct ext2_inode*) buffer;

      if (inode_ptr->i_mode == 0 || inode_ptr->i_links_count == 0)
	continue;

      printf("INODE,");
      //inode number
      printf("%d,", i+1);
      int fileMode = inode_ptr->i_mode;

      char file_type;
      //file type
      if (fileMode & 0x4000)
	{
	  printf("d,");
	  file_type = 'd';
	}
      else if (fileMode & 0x8000)
	{
	  printf("f,");
	  file_type = 'f';
	}
      else if (fileMode & 0xA000)
	{
	  printf("s,");
	  file_type = 's';
	}
      else
	{
	  printf("?,");
	  file_type = '?';
	}
      
      //mode number
      printf("%o,", inode_ptr->i_mode & 4095);
      // owner
      printf("%d,", inode_ptr->i_uid);
      // group
      printf("%d,", inode_ptr->i_gid);
      // link count
      printf("%d,", inode_ptr->i_links_count);
      // time of last inode change/when inode is created
      convert_time(inode_ptr->i_ctime, create_buffer);
      printf("%s,", create_buffer);
      // time of last modification
      convert_time(inode_ptr->i_mtime, modified_buffer);
      printf("%s,", modified_buffer);
      //time of last access
      convert_time(inode_ptr->i_atime, accessed_buffer);
      printf("%s,", accessed_buffer);
      //file size
      printf("%d,", inode_ptr->i_size);
      //number of blocks
      printf("%d,", inode_ptr->i_blocks);
      //fifteen block addresses
      int j;
      for (j = 0; j < 15; j++)
	{
	  if (j != 14)
	    printf("%d,", inode_ptr->i_block[j]);
	  else
	    printf("%d\n", inode_ptr->i_block[j]);
	}
      if (file_type == 'd' || file_type == 'f')
	{
	  directory_entry(inode_ptr, i+1, file_type);
	}
    }  
}
int main(int argc, char* argv[])
{
  if (argc != 2)
    {
      fprintf(stderr, "Wrong number of parameters!");
      exit(1);
    }

  ext2fd = open (argv[1], O_RDONLY);
  if (ext2fd < 0)
    {
      fprintf(stderr, "Error opening disk image!");
      exit(1);
    }
  superblock_summary();
  group_summary();
  free_blocks();
  free_inodes();
  inode_summary();
  return 0;
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}
