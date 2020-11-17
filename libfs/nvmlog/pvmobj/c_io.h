#ifndef C_IO_H_
#define C_IO_H_


//#ifdef __cplusplus
//extern "C" {
//#endif
//typedef intptr_t nvword_t

int write_io_( float *f, int *elements, int *num_proc, int *iid);
void *nv_restart_(char *var, int *id);
int nvchkpt_all_(int *mype);

//void* my_alloc_(unsigned int* n, char *s, int *iid);
//void* alloc_( unsigned int size, char *var, int id, int commit_size);


//Non persistent malloc
void* npvalloc_( size_t size);
void* nvread_(char *var, int id);
void* nvread_id_(unsigned int varid, int id);
void npvfree_(void *mem);
void npv_c_free_(void *mem);
void* nv_shadow_copy(void *src_ptr, size_t size, char *var, int id, size_t commit_size);
void nv_renameobj(char *src, char *dest);
void test1();

/*C Interface*/
void* npv_c_alloc_( size_t size, unsigned long *ptr);
void* p_c_nvread_(char *var, int id);

//returns also length of chunk
void* p_c_nvread_len(char *var, int id, size_t *chunksize);
void* nvread_len(char *var, int id, size_t *chunksize);
void* nvread(char *var, int id);
void nvdelete(char *var, int id);

//Both same interface. one for c and other for c++
void* nvalloc_( size_t size, char *var, int id);
void *nvalloc_id(size_t size, char *var, unsigned int *objid);


void* nvallocref_(size_t size, char *var, int id, unsigned long *ptr);
void nvfree_(void *var);
void mmap_free(char *varname, void *ptr);


void* p_c_nvalloc_( size_t size, char *var, int id);
void *p_c_nvalloc_id(size_t size, char *var, unsigned int *objid);
void p_c_free_(void *ptr);

void p_c_mmap_free(char *varname, void *ptr);

/////////////////////COMMIT FUNCTIONS/////////////////
int p_c_nvcommit(size_t size, char *var, int id);
int p_c_nvcommitobj(void *addr, int id);
void  p_c_nvcommitsz(char *ptr, size_t commitsz);
void p_c_nvcommitsz_id(unsigned int objid,size_t commitsz);

int nvcommitobj(void *addr, int id);
#define NVCOMMITOBJ(addr, id)  nvcommitobj(addr, id);

int nvcommit_(size_t size, char *var, int id);
void nvcommitsz(char *ptr, size_t commitsz);
void nvcommitsz_id(unsigned int objid,size_t commitsz);

//need to add for c++ apps
int nvcommitword_(void *wordaddr);

int nvsync(void *ptr, size_t len);
#define NVSYNC(addr, len) nvsync(addr, len);

char** get_object_name_list(int pid, int *entries);

/////////////////////TRANSACTION FUNCTIONS/////////////////


#define BEGIN_OBJTRANS(addr, pid)  begin_trans_obj(addr, pid);
#define C_BEGIN_OBJTRANS(addr, pid)  c_begin_trans_obj(addr, pid);

#define BEGIN_WRDTRANS(addr, pid, size)  begin_trans_wrd(addr, pid, size);
#define C_BEGIN_WRDTRANS(addr, pid, size)  c_begin_trans_wrd(addr, pid,size);

/*uses object addr to start transaction*/
int begin_trans_obj(void *addr, int pid);
int c_begin_trans_obj(void *addr, int pid);

/*uses word addr to start transaction*/
int begin_trans_wrd(void *addr, size_t size, int pid);
int c_begin_trans_wrd(void *addr, size_t size, int pid);

/////////////////////REDO LOG FUNCTIONS/////////////////

//int store_word(nvword_t *addr, nvword_t value);
//nvword_t load_word(nvword_t *addr);


/////////////////RESTART RELATED/////////////////////////
void *load_ptr(void **ptr);
void *c_load_ptr(void **ptr);

#define LOADNVPTR(ptr) load_ptr(ptr);
#define C_LOADNVPTR(ptr) c_load_ptr(ptr);





#ifdef _USE_CHECKPOINT
int* create_shadow(int*&, int, char const*, int);
int** create_shadow(int**&, int, int, char const*, int);
double* create_shadow(double*&, int, char const*, int);
double** create_shadow(double**&, int, int, char const*, int);	

#endif


/////////////////APP START AND TERMINATION////////////////////
unsigned int BASEID_GET();
int app_start(unsigned int pid);
int c_app_stop_(int mype);
int app_stop_(unsigned int mype);

#define NVINIT(pid) nvinit(pid);
int nvinit(unsigned int pid);
int nvinit_(unsigned int pid);

//////////////////////////////////////////////////////////////


//void test();
//#ifdef __cplusplus
//}
//#endif

#endif
