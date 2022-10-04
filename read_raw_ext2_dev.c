#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>          /* open */
#include <stdio.h>          /* printing */
#include <stdlib.h>         /* exit */
#include <limits.h>         /* CHAR_BIT */
#include <unistd.h>         /* close, lseek */
#include <string.h>         /* memcpy */
#include <ext2fs/ext2_fs.h> /* ext2_super_block */

#define USB_DEVICE          "/dev/sdb1"
#define BASE_OFFSET         1024

/*
  From an index into the block group descriptor list,
  calculate the starting byte
*/
#define GROUP_OFFSET(index)  (BASE_OFFSET + block_size + \
                              (index * sizeof(struct ext2_group_desc)))

/*
  From a block number, calculates the starting byte of that block
  Blocks are numbered from 1 and block 1 is the super-block so need
  to take that into consideration
 */
#define BLOCK_OFFSET(block)   (BASE_OFFSET + (block - 1) * block_size)

static void read_super_block(int fd, struct ext2_super_block *super);
static void read_group_descriptor(int fd, int index, struct ext2_group_desc *group);
static void read_block_bitmap(int fd, struct ext2_group_desc *group);
static void read_inode(int fd, int inode_no, const struct ext2_group_desc *group,
                       struct ext2_inode *inode);
static void read_dir(int fd, const struct ext2_inode *inode,
                     const struct ext2_group_desc *group);
// static void read_inode_table(int fd, const struct ext2_group_desc *group);
static void bit_print(unsigned char a);
static unsigned int block_size = 0; /* block size (to be calculated) */
static unsigned short inode_size; /* size of inode, to be obtained from super block */
static unsigned int inodes_per_group;

/*
  USB device - mke2fs output:
  Creating filesystem with 124908 1k blocks and 31232 inodes
  Superblock backups stored on blocks: 
	8193, 24577, 40961, 57345, 73729
 */

int main(void)
{
  struct ext2_super_block super;
  struct ext2_group_desc  group;
  struct ext2_inode       inode;
  int                     fd;

  if ((fd = open(USB_DEVICE, O_RDONLY)) < 0) {
    perror(USB_DEVICE);
    exit(EXIT_FAILURE);
  }

  read_super_block(fd, &super);
  read_group_descriptor(fd, 0, &group);
  read_block_bitmap(fd, &group);
  read_inode(fd, 36, &group, &inode);
  // read_inode_table(fd, &group);
  read_dir(fd, &inode, &group);
  
  close(fd);
  return EXIT_SUCCESS;
}

static void read_super_block(int fd, struct ext2_super_block *super)
{
  off_t seek;
  ssize_t read_ret;

  /*
    Superblock is located at offset 1024 on USB so first seek to that
    position and then read from there
   */
  seek = lseek(fd, BASE_OFFSET, SEEK_SET);
  printf("seek on super block: %li\n", seek);
  read_ret = read(fd, super, sizeof(struct ext2_super_block));
  printf("read on super block: %zi\n", read_ret);
  

  if (super->s_magic != EXT2_SUPER_MAGIC) {
    fprintf(stderr, "Not a ext2 filesystem!\nsuper.s_magic: %i\n", super->s_magic);
    exit(EXIT_FAILURE);
  }

  /*
    From output of mke2fs we know:
      - inodes count is 31232
      - blocks count is 124908
      - block size is 1KB (1024)
      - blocks count * block size = 127906816 (128MB - size of USB)

    Struct will tell us:
      - size of a block, expressed as a power of 2 using 1024 bytes as a unit.
        Thus 0 denotes 1024-bytes blocks, 1 denotes 2048-bytes block etc
        To calculate size of bytes in a block use bit shifting as below.
      - Number of blocks per group (8192 on the USB)

    For the USB we have 8192 blocks per group and 124908 blocks. Therefore we
    have 124908 / 8192 = 15.24 block groups, rounded up to 16.
   */
  
  block_size = 1024 << super->s_log_block_size;
  inodes_per_group = super->s_inodes_per_group;
  inode_size = super->s_inode_size;

  printf("Reading super-block from device " USB_DEVICE ":\n"
         "Inodes count              : %u\n"
         "Blocks count              : %u\n"
         "Reserved blocks count     : %u\n"
         "Free blocks count         : %u\n"
         "Free indoes count         : %u\n"
         "First data block          : %u\n"
         "Block size                : %u\n"
         "Blocks per group          : %u\n"
         "Inodes per group          : %u\n"
         "Creator OS                : %u\n"
         "First non-reserved inode  : %u\n"
         "Size of inode structure   : %hu\n"
         ,
         super->s_inodes_count,
         super->s_blocks_count,
         super->s_r_blocks_count,
         super->s_free_blocks_count,
         super->s_free_inodes_count,
         super->s_first_data_block,
         block_size,
         super->s_blocks_per_group,
         super->s_inodes_per_group,
         super->s_creator_os,
         super->s_first_ino,
         inode_size
         );
}

/*
  Blocks immediately following the super-block reside the list of block group descriptors.
  Reads a block group descriptor at a specified index into that list and prints out some
  useful fields.
 */
static void read_group_descriptor(int fd, int index, struct ext2_group_desc *group)
{
  off_t seek;
  ssize_t read_ret;
  int offset;

  offset = GROUP_OFFSET(index);
  seek = lseek(fd, offset, SEEK_SET);
  printf("\nseek on group descriptor: %li\n", seek);
  read_ret = read(fd, group, sizeof(struct ext2_group_desc));
  printf("read on group descriptor: %zi\n", read_ret);

  /*
    The struct tell us which blocks the bitmaps reside
   */
  printf("Reading group descriptor (%i) from device " USB_DEVICE ":\n"
         "Blocks bitmap block: %u\n"
         "Inodes bitmap block: %u\n"
         "Inodes table block:  %u\n"
         "Free blocks count:   %u\n"
         "Free inodes count:   %u\n"
         "Directories count:   %u\n"
         ,
         index,
         group->bg_block_bitmap,
         group->bg_inode_bitmap,
         group->bg_inode_table,
         group->bg_free_blocks_count,
         group->bg_free_inodes_count,
         group->bg_used_dirs_count);
}


/*
  Read the block bitmap for a group descriptor. Each bit in the map signifies
  whether the block is in use or not (1 for in-use, 0 otherwise).
 */
static void read_block_bitmap(int fd, struct ext2_group_desc *group)
{
  off_t seek;
  ssize_t read_ret;
  int i;
  unsigned char *bitmap;

  bitmap = (unsigned char *) malloc(block_size); /* allocate memory for the bitmap */
  seek = lseek(fd, BLOCK_OFFSET(group->bg_block_bitmap), SEEK_SET);
  printf("seek on block bitmap: %li\n", seek);
  /* read block bitmap, its size must fit in one block */
  read_ret = read(fd, bitmap, block_size);
  printf("read on block bitmap: %zi\n", read_ret);

  for(i = 0; i < block_size; i++) {
    printf("Byte %4i: ", i);
    bit_print(*(bitmap + i));
  }

  free(bitmap);
}

static void read_inode(fd, inode_no, group, inode)
     int                          fd;
     int                          inode_no;
     const struct ext2_group_desc *group;
     struct ext2_inode            *inode;
{
  off_t seek, inode_offset;
  ssize_t read_ret;
  int i, index, block_no, shift;
  void *block;

  index = (inode_no - 1) % inodes_per_group;
  block_no = (index * inode_size) / block_size;
  shift = (index * inode_size) / block_size;
  
  // inode_offset = BLOCK_OFFSET(group->bg_inode_table) + ((index * inode_size) / block_size);
  inode_offset = BLOCK_OFFSET(group->bg_inode_table) + (block_no * block_size); 
  seek = lseek(fd, inode_offset, SEEK_SET);
  printf("\nseek of inode %i: %li\n", inode_no, seek);
  // read_ret = read(fd, inode, sizeof(struct ext2_inode));
  // printf("read on inode %i: %zi\n", inode_no, read_ret);
  read_ret = read(fd, block, block_size);
  printf("read on block number %i: %zi\n", block_no, read_ret);
  inode = (struct ext2_inode *) (block << shift);

  printf("Reading root inode:\n"
         "File mode: %hu\n"
         "Owner UID: %hu\n"
         "Size     : %u bytes\n"
         "Blocks   : %u\n",
         inode->i_mode,
         inode->i_uid,
         inode->i_size,
         inode->i_blocks);

  for (i = 0; i < EXT2_N_BLOCKS; i++)
    if (i < EXT2_NDIR_BLOCKS)
      printf("Block %2u : %u\n", i, inode->i_block[i]);
    else if (i == EXT2_IND_BLOCK)
      printf("Single    : %u\n", inode->i_block[i]);
    else if (i == EXT2_DIND_BLOCK)
      printf("Double    : %u\n", inode->i_block[i]);
    else if (i == EXT2_TIND_BLOCK)
      printf("Triple    : %u\n", inode->i_block[i]);

  if ((block = malloc(block_size)) == NULL) {
    fprintf(stderr, "Memory error\n");
    close(fd);
    exit(1);
  }

  lseek(fd, BLOCK_OFFSET(inode->i_block[12]), SEEK_SET);
  read(fd, block, block_size);
  printf("indirect blocks: %i, %i, %i\n", *((int *) block), *((int *) block + 3), *((int *) block + 13));

  lseek(fd, BLOCK_OFFSET(*((int *) block + 13)), SEEK_SET);
  read(fd, block, block_size);
  printf("content:\n%s", (char *) block);
        

  free(block);
}

static void read_dir(int fd, const struct ext2_inode *inode,
                     const struct ext2_group_desc *group)
{
  void *block;
  struct ext2_dir_entry_2 *entry;
  unsigned int size = 0;

  if (S_ISDIR(inode->i_mode)) {
  
    if ((block = malloc(block_size)) == NULL) {
      fprintf(stderr, "Memory error\n");
      close(fd);
      exit(1);
    }

    lseek(fd, BLOCK_OFFSET(inode->i_block[4]), SEEK_SET);
    read(fd, block, block_size);

    entry = (struct ext2_dir_entry_2 *) block; /* first entry in the directory */
    printf("entry->inode: %u\n", entry->inode);
    while ((size < inode->i_size) && entry->inode) {
      char file_name[EXT2_NAME_LEN + 1];
      memcpy(file_name, entry->name, entry->name_len);
      file_name[entry->name_len] = '\0';
      printf("%10u %s %hi\n", entry->inode, file_name, entry->rec_len);
      entry = (struct ext2_dir_entry_2 *) ((void *) entry + entry->rec_len);
      size += entry->rec_len;
    }
    free(block);
  }
}


/*
static void read_inode_table(int fd, const struct ext2_group_desc *group)
{
  struct ext2_inode inode;
  off_t seek, inode_offset;
  ssize_t read_ret;
  int i;

  for (i = 1; i <= 30; i++) {
    inode_offset = BLOCK_OFFSET(group->bg_inode_table) + ((i - 1) * inode_size);
    seek = lseek(fd, inode_offset, SEEK_SET);
    read_ret = read(fd, &inode, sizeof(struct ext2_inode));

    printf("inode %i\nSize: %hu bytes\n", i, inode.i_size);
  }
}
*/

/*
  Print out a char as a bit string. Makes it easier to
  visualise bit maps.
 */
static void bit_print(unsigned char a)
{
  int i;
  int n = sizeof(char) * CHAR_BIT;
  unsigned char mask = 1 << (n - 1);

  for (i = 1; i <= n; ++i) {
    putchar(((a & mask) == 0) ? '0' : '1');
    a <<= 1;
  }
  putchar('\n');
}
