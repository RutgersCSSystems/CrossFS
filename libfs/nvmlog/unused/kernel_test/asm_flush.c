#include <stdio.h>
#include <stdlib.h>
char addr[256];
#define MAXSIZE 1024*1024*4

int main() {

	volatile unsigned long *addr1 = (unsigned long)malloc(sizeof(unsigned long) * MAXSIZE);
	volatile unsigned int size=0;
	int i=0;

	 volatile unsigned long *addr = addr1;
	 while(size < MAXSIZE){
	   //asm volatile("clflush %0" : "+m" (addr));
	   asm volatile ("clflush %0\n" : "+m" (*(char *)(addr)));
	   addr= addr+64;
	   size += 64;		
	 }

	 size = 0;
	 addr = addr1;	
	 while(size < MAXSIZE){
	   addr= addr+64;
	   volatile unsigned char *temp = addr;	
	   volatile unsigned char s;	 
	   for(i=0;i<64;i++){
		s = *temp;
		s = s +1;
		temp++;	
	   }
	   s = s+10;			
	   size += 64;		
	 }
}


