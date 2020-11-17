#ifndef C_IO_H_
#define C_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

int write_io_( float *f, int *elements, int *num_proc, int *iid);
void *nv_restart_(char *var, int *id);
int nvchkpt_all_(int *mype);

void* my_alloc_(unsigned int* n, char *s, int *iid);
void* alloc_( unsigned int size, char *var, int id, int commit_size);
void* nvalloc_( size_t size, char *var, int id);
//Non persistent malloc
void* npvalloc_( size_t size);
void* npv_c_alloc_( size_t size, unsigned long *ptr);
void* npv_c_realloc_( void*ptr, size_t size);

void* nvread_(char *var, int id);
void npvfree_(void *mem);
void npv_c_free_(void *mem);

void* nvread(char *var, int id);

void* nv_shadow_copy(void *src_ptr, size_t size, char *var, int id, size_t commit_size);
void test1();

/*int* create_shadow(int*&, int, char const*, int);
int** create_shadow(int**&, int, int, char const*, int);
double* create_shadow(double*&, int, char const*, int);
double** create_shadow(double**&, int, int, char const*, int);*/
int app_stop_(int *mype);




//void test();
#ifdef __cplusplus
}
#endif
#endif
