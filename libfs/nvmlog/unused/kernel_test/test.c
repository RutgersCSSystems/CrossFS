#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>


#define __NR_NValloc 338

#define __NR_NVwrite 339

#define __NR_brk     45

#define __NR_nvpoolcreate       340

#define __NR_nvprocexit 342

int has_initialized = 0;
void *managed_memory_start;
void *last_valid_address;

struct mem_control_block { 
 int is_available; 
 int size;
};


unsigned long  malloc_init_test( unsigned long val  ){
 
      return (unsigned long) syscall(__NR_NValloc, val);    

      //return (unsigned long) syscall(__NR_brk, val);
}


unsigned long  malloc_init_pool( unsigned long num_pages  ){

      return (unsigned long) syscall(__NR_nvpoolcreate, num_pages);

}


void malloc_init()
{ 
 /* grab the last valid address from the OS */  
 //last_valid_address =  syscall(__NR_NValloc, 1);//  sbrk(0);     

 //syscall(__NR_brk, 0);

 /* we don't have any memory to manage yet, so 
  *just set the beginning to be last_valid_address 
  */  
 //managed_memory_start = last_valid_address;     

 /* Okay, we're initialized and ready to go */
  //has_initialized = 1;   
}

void *malloc_mem(long numbytes) { 
 /* Holds where we are looking in memory */ 
 void *current_location; 

 /* This is the same as current_location, but cast to a 
  * memory_control_block 
  */
 struct mem_control_block *current_location_mcb;  

 /* This is the memory location we will return.  It will 
  * be set to 0 until we find something suitable 
  */  
 void *memory_location;  

 /* Initialize if we haven't already done so */
 if(! has_initialized)  { 
  malloc_init();
 }

 /* The memory we search for has to include the memory 
  * control block, but the user of malloc doesn't need 
  * to know this, so we'll just add it in for them. 
  */
 numbytes = numbytes + sizeof(struct mem_control_block);  

 /* Set memory_location to 0 until we find a suitable 
  * location 
  */
 memory_location = 0;  

 /* Begin searching at the start of managed memory */ 
 current_location = managed_memory_start;  

 /* Keep going until we have searched all allocated space */ 
 while(current_location != last_valid_address)  
 { 
  /* current_location and current_location_mcb point
   * to the same address.  However, current_location_mcb
   * is of the correct type so we can use it as a struct.
   * current_location is a void pointer so we can use it
   * to calculate addresses.
   */
  current_location_mcb = 
   (struct mem_control_block *)current_location;

  if(current_location_mcb->is_available)
  {
   if(current_location_mcb->size >= numbytes)
   {
    /* Woohoo!  We've found an open, 
     * appropriately-size location.  
     */

    /* It is no longer available */
    current_location_mcb->is_available = 0;

    /* We own it */
    memory_location = current_location;

    /* Leave the loop */
    break;
   }
  }

  /* If we made it here, it's because the Current memory 
   * block not suitable, move to the next one 
   */
  current_location = current_location + 
   current_location_mcb->size;
 }

 /* If we still don't have a valid location, we'll 
  * have to ask the operating system for more memory 
  */
 if(! memory_location)
 {
  /* Move the program break numbytes further */
  //sbrk(numbytes);
   //syscall(__NR_NValloc, numbytes); 
     syscall(__NR_brk,numbytes);

  /* The new memory will be where the last valid 
   * address left off 
   */
  memory_location = last_valid_address;

  printf("Comes till here \n");

  /* We'll move the last valid address forward 
   * numbytes 
   */
  last_valid_address = last_valid_address + numbytes;

  printf("Comes till here 0.5 \n");

  /* We need to initialize the mem_control_block */
  current_location_mcb = memory_location;

  printf("Comes till here 0.6\n");
 current_location_mcb->is_available = 0;
 
  printf("Comes till here 0.7 \n");
  current_location_mcb->size = numbytes;
 }

 printf("Comes till here1 \n");

 /* Now, no matter what (well, except for error conditions), 
  * memory_location has the address of the memory, including 
  * the mem_control_block 
  */ 

 /* Move the pointer past the mem_control_block */
 memory_location = memory_location + sizeof(struct mem_control_block);


 printf("Comes till here 0.8\n");

 /* Return the pointer */
 return memory_location;
 }


void test_persistence(unsigned long addr, int offset) {

       char *alloc_start;
       
       alloc_start = addr;

      printf("test_persistence %c \n", alloc_start[offset]);

      return;

}


int main(){

        char *alloc_start, *alloc_end, *addr3;
        unsigned long temp; 
        int i =0;

        //malloc_init_pool(10);
        
       //test_persistence ( 155987968,  3000);   
       //return 0;

        alloc_start=  malloc_init_test(0);
        if (!alloc_start)
           return 0;

      alloc_end = alloc_start + 4095;


      temp  = malloc_init_test( alloc_end );
      printf("alloc_start is %ld alloc_end is %ld \n",alloc_start, alloc_end);

      //alloc_start[3000] ='a';
      //sleep(10);

      alloc_start = alloc_end; 
      alloc_end =  alloc_start + 4095;

      temp = malloc_init_test( alloc_end );
      if ( !temp )
          return 0;         
     
      
      alloc_start[1000] = 'c';

      printf("Your __NR_NValloc system call returned the value %c\n", alloc_start[1000] );

       sleep(10);
 
      //(unsigned long) syscall(__NR_nvprocexit, 1);       


       //sleep(10);
       /*char *a = malloc(100);
        a[8090] = 'b';
         printf("a %s\n",a);*/

 return 0;
}
