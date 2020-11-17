
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

void ** create_memory(int nranks, int myrank, size_t);
void** group_create_memory(int nranks, int myrank, size_t);
int armci_remote_memcpy(int myrank, int my_peer, void **ptr, void *, size_t bytes);
int invoke_barrier();
int create_group ( int *members, int cnt, int myrank,  int numrank);
int coordinate_chunk(int chunk, int mypeer, int myrank);
#ifdef __cplusplus
}
#endif
