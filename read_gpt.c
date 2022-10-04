#include <fcntl.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define USB_DEVICE "/dev/sdb"
#define BASE_OFFSET 512

struct header {
  unsigned long magic;
  unsigned int  revision;
};
typedef struct header header;

int
main(void)
{

  int fd;
  off_t seek_ret;
  ssize_t read_ret;
  header h;
  //char magic[8];
  //char revision[4];
  //long int i_revision;
  //char header[4];
  //int i;

  /*
    unsigned short revision_first = 0;
    unsigned short revision_second = 0;
    unsigned short header_size_first = 0;
    unsigned short header_size_second = 0;
  */

  printf("header:%3lu bytes\n", sizeof(header));
  if ((fd = open(USB_DEVICE, O_RDONLY)) < 0) {
    perror(USB_DEVICE);
    exit(EXIT_FAILURE);
  }

  seek_ret = lseek(fd, BASE_OFFSET, SEEK_SET);
  printf("lseek: %li\n", seek_ret);
  read_ret = read(fd, &h, sizeof(header));
  printf("read: %zi\n", read_ret);
  printf("magic: %lx\n", h.magic);
  printf("revision: %x\n", h.revision);
  
  /*
  seek_ret = lseek(fd, BASE_OFFSET, SEEK_SET);
  printf("lseek: %li\n", seek_ret);
  read_ret = read(fd, &magic, 8 * sizeof(char));
  printf("read: %zi\n", read_ret);
  printf("magic: %s\n", magic);

  read_ret = read(fd, &revision, 4 * sizeof(char));
  printf("read: %zi\n", read_ret);

  
  if (revision[0] == 0x00) {
    printf("revision[0] is 0x00\n");
    revision[0] = '0';
  }

  if (revision[1] == 0x00) {
    printf("revision[1] is 0x00\n");
    revision[1] = '0';
  }
    
  if (revision[2] == 0x01) {
    printf("revision[2] is 0x01\n");
    revision[2] = '1';
  }

  if (revision[3] == 0x00) {
    printf("revision[3] is 0x00\n");
    revision[3] = '0';
  }
  
  revision[4] = '\0';
  i_revision = strtol(revision, NULL, 16);
  printf("i_revision: %li\n", i_revision);
  
  read_ret = read(fd, &header, 4 * sizeof(char));
  printf("read: %zi\n", read_ret);

  if (header[0] == 0x5c)
    printf("header[0] is %hx\n", header[0]);

  if (header[1] == 0x00)
    printf("header[1] is 0x00\n");

  if (header[2] == 0x00)
    printf("header[2] is 0x00\n");

  if (header[3] == 0x00)
      printf("header[3] is 0x00\n");
  
  read_ret = read(fd, &revision_first, sizeof(short));
  printf("read: %zi\n", read_ret);
  printf("revision_first: %i\n", revision_first);

  read_ret = read(fd, &revision_second, sizeof(short));
  printf("read: %zi\n", read_ret);
  printf("revision_second: %i\n", revision_second);
  
  read_ret = read(fd, &header_size_first, sizeof(short));
  printf("read: %zi\n", read_ret);
  printf("header_size_first: %i\n", header_size_first);

  read_ret = read(fd, &header_size_second, sizeof(short));
  printf("read: %zi\n", read_ret);
  printf("header_size_second: %i\n", header_size_second);
  */
  close(fd);

  exit(EXIT_SUCCESS);
}
