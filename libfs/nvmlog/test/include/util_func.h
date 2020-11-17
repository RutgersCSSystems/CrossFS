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

int check_modify_access(int perm);
long simulation_time(struct timeval start, struct timeval end );
int memcpy_delay(void *dest, void *src, size_t len);
int memcpy_delay_temp(void *dest, void *src, size_t len);

int update_content_hash(void *src_buff);
int compare_content_hash( void *src, void *dest, size_t length);
int hash_clear();

void add_bw_timestamp( int id, struct timeval st,
                     struct timeval end, size_t bytes );

void print_all_bw_timestamp();
