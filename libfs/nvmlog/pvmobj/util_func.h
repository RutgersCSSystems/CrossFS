#include <sys/time.h>

struct BWSTAT {
    struct timeval start_time;
    struct timeval end_time;
    size_t bytes;
};

int rand_word(char *input, int len);
unsigned int gen_id_from_str(char *key);
void sha1_mykeygen(void *key, char *digest, size_t size,
                    int base, size_t input_size);
int compare_checksum(void *src, size_t len, long refval);
long generate_checksum(void *src, size_t len);
int check_proc_perm(int perm);
long simulation_time(struct timeval start, struct timeval end );
int memcpy_delay(void *dest, void *src, size_t len);
int memcpy_delay_temp(void *dest, void *src, size_t len);
int update_content_hash(void *src_buff);
int compare_content_hash( void *src, void *dest, size_t length);
int hash_clear();
void add_bw_timestamp( int id, struct timeval st,
                     struct timeval end, size_t bytes );
void print_all_bw_timestamp();
int create_map_file(char *basepath, int pid, char *out_fname,
                int vmaid);
char* generate_file_name(char *base_name, int pid, char *dest);
int gen_rand(int max, int min);

/*POSIX wrappers*/
int posix_fileopen(char *base_name, const char *mode);
void posix_close(int fd);
int setup_map_file(char *filepath, unsigned int bytes);
int check_existing_map_file(char *filepath);

void objnamemap_insert( char *key, int val);
void objnamemap_increment(char *key);
int objnamemap_find(char *key);
int objnamemap_clear();
void objnamemap_delete(char *key);
size_t find_objnamemap_total();
char** get_object_list(int *count);
