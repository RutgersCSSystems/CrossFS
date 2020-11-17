#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

main(void) {

  size_t bytesWritten = 0;
  int   my_offset = 0;
  char  *text1="Data for file 1";
  char  *text2="Data for file 2";
  int fd1,fd2;
  int PageSize;
  void *address;
  void *address2;
  fd1 = open("/tmp/mmaptest1",O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
  if ( fd1 < 0 )
    perror("open() error");
  else {
    bytesWritten = write(fd1, text1, strlen(text1));
    if ( bytesWritten != strlen(text1) ) {
      perror("write() error");
      int closeRC = close(fd1);
      return -1;
    }

  fd2 = open("/tmp/mmaptest2", O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    if (fd2 < 0 )
      perror("open() error");
    else {
      bytesWritten = write(fd2, text2, strlen(text2));
      if ( bytesWritten != strlen(text2) )
        perror("write() error");

      PageSize = (int)sysconf(_SC_PAGESIZE);
      if ( PageSize < 0) {
        perror("sysconf() error");
      }
      else {

      off_t lastoffset = lseek( fd1, PageSize-1, SEEK_SET);
      if (lastoffset < 0 ) {
        perror("lseek() error");
      }
      else {
      bytesWritten = write(fd1, " ", 1);   /* grow file 1 to 1 page. */

      off_t lastoffset = lseek( fd2, PageSize-1, SEEK_SET);

      bytesWritten = write(fd2, " ", 1);   /* grow file 2 to 1 page. */
        /*
         *  We want to show how to memory map two files with
         *  the same memory map.  We are going to create a two page
         *  memory map over file number 1, even though there is only
         *  one page available. Then we will come back and remap
         *  the 2nd page of the address range returned from step 1
         *  over the first 4096 bytes of file 2.
         */

       int len;

       my_offset = 0;
       len = PageSize;   /* Map one page */
       address = mmap(NULL,
                      len,
                      PROT_READ,
                      MAP_SHARED,
                      fd1,
                      my_offset );
       if ( address != MAP_FAILED ) {
         address2 = mmap( ((char*)address)+PageSize,
                       len,
                       PROT_READ,
                       MAP_SHARED | MAP_FIXED, fd1,
                       my_offset );
         if ( address2 != MAP_FAILED ) {
               /* print data from file 1 */
            printf("\n%s",address);
               /* print data from file 2 */
            printf("\n%s",address2);
         } /* address 2 was okay. */
         else {
           perror("mmap() error=");
         } /* mmap for file 2 failed. */
       }
       else {
         perror("munmap() error=");
       }
         /*
          *  Unmap two pages.
          */
       if ( munmap(address, 2*PageSize) < 0) {
        perror("munmap() error");
      }
      else;

     }
    }
    close(fd2);
    unlink( "/tmp/mmaptest2");
    }
    close(fd1);
    unlink( "/tmp/mmaptest1");
  }
     /*
      *  Unmap two pages.
      */
  if ( munmap(address, 2*PageSize) <    0) {
     perror("munmap() error");
  }
  else;
}
