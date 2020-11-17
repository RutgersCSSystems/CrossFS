#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#ifdef _VALIDATE_CHKSM
#include <openssl/evp.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <pthread.h>
#include <iostream>
#include <map>
#include <vector>

#include "nv_def.h"
#include "time_delay.h"
#include "util_func.h"

using namespace std;
#define UNIT_SIZE 4096
#define MAXOBJLEN 255

extern pthread_mutex_t chkpt_mutex;

map<std::string,int> objnamemap;
map<std::string,int>::iterator objmap_it;


/* assembly code to read the TSC */
static inline uint64_t RDTSC()
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}

/* To calculate simulation time */
long simulation_time(struct timeval start, struct timeval end )
{
	long current_time;

	current_time = ((end.tv_sec * 1000000 + end.tv_usec) -
			(start.tv_sec*1000000 + start.tv_usec));

	return current_time;
}



int gen_rand(int max, int min)
{
	int n=0;
	n=(rand()%((max-min))+min);
	return(n);
}


int rand_word(char *input, int len) {

	int i = 0;
	int max = 122;
	int min = 97;

	memset(input, 0, len);
	while ( i < len) {
		input[i] = (char)(gen_rand(max, min));
		i++;
	}
	input[i] = 0;
	return 0;
}


//typedef unsigned __int64 uint64_t;
uint64_t MurmurHash64A ( const void * key, int len, unsigned int seed )

{

	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;
	uint64_t h = seed ^ (len * m);
	const uint64_t * data = (const uint64_t *)key;
	const uint64_t * end = data + (len/8);
	while(data != end)

	{
		uint64_t k = *data++;
		k *= m; 
		k ^= k >> r; 
		k *= m; 
		h ^= k;
		h *= m; 
	}

	const unsigned char * data2 = (const unsigned char*)data;
	switch(len & 7)
	{
	case 7: h ^= uint64_t(data2[6]) << 48;
	case 6: h ^= uint64_t(data2[5]) << 40;
	case 5: h ^= uint64_t(data2[4]) << 32;
	case 4: h ^= uint64_t(data2[3]) << 24;
	case 3: h ^= uint64_t(data2[2]) << 16;
	case 2: h ^= uint64_t(data2[1]) << 8;
	case 1: h ^= uint64_t(data2[0]);
	h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;
	return h;
}

unsigned int jenkin_hash(char *key, unsigned int len)
{
	uint32_t hash, i;
	for(hash = i = 0; i < len; ++i)
	{
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return (unsigned int)hash;
}


static unsigned int  sdbm(char *str)
{
	unsigned int hash = 0;
	int c;

	while (c = *str++)
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}


unsigned int gen_id_from_str(char *key)
{
	//return jenkin_hash(key, len)
			//return MurmurHash64A(key,len,10030304);
	return sdbm(key);
}


// Slight variation on the ETH hashing algorithm
//static int magic1 = 1453;
static int MAGIC1 = 1457;
unsigned int gen_id_from_str1(char *key) {

	long hash = 0;

	while (*key) {
		hash += ((hash % MAGIC1) + 1) * (unsigned char) *key++;

	}
	return hash % 1699;
}

#ifdef _VALIDATE_CHKSM

void convert_base16 (unsigned char num, char *out)
{
	unsigned char mask = 15;
	int i = 0;
	for (i = 1; i >= 0; i--)
	{
		int digit = num >> (i * 4);
		sprintf (out, "%x", digit & mask);
		out++;
	}
	*out = '\0';
}

void sha1_mykeygen(void *key, char *digest, size_t size,
		int base, size_t input_size) {

	EVP_MD_CTX mdctx;
	const EVP_MD *md;
	unsigned char *md_value = (unsigned char *) malloc(EVP_MAX_MD_SIZE
			* sizeof(unsigned char));
	//unsigned char md_value[EVP_MAX_MD_SIZE];
	assert(md_value);
	assert(key);
	assert(digest);

	unsigned int md_len, i;
	char digit[10];
	char *tmp;

	if (digest == NULL) {
		perror("Malloc error!");
		exit(1);
	}
	memset(digest, 0, size);
	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("sha1");
	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);
	EVP_DigestUpdate(&mdctx, key, input_size);
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

	tmp = digest;
	for (i = 0; i < md_len; i++) {
		convert_base16(md_value[i], digit);
		strcat(tmp, digit);
		tmp = tmp + strlen(digit);
	}
	tmp = '\0';
	digest[size] = '\0';

	//free(md_value);
	////fprintf(stdout,"sha1_mykeygen: %s \n",digest);
}


long generate_checksum(void *src, size_t len){

	char gen_key[256];
	long hash;

	bzero(gen_key, 256);
	sha1_mykeygen(src, gen_key,CHKSUM_LEN, 16, len);
	hash = gen_id_from_str(gen_key);
	return hash;
}

int compare_checksum(void *src, size_t len, long refval){

	if(generate_checksum(src, len) != refval) {
		return -1;
	}
	fprintf(stderr,"checksums match %u\n", refval);
	return 0;
}


#endif

int check_proc_perm(int perm) {
#ifdef _USE_SHADOWCOPY
	return 0;
#else
	return 1;
#endif
	return 0;
}


int __nsleep(const struct timespec *req, struct timespec *rem)
{
	struct timespec temp_rem;
	if(nanosleep(req,rem)==-1)
		__nsleep(rem,&temp_rem);
	else
		return 1;
}

int msleep(unsigned long nanosec)
{
	struct timespec req={0},rem={0};
	time_t sec=(int)(nanosec/1000000000);
	//milisec=milisec-(sec*1000);
	req.tv_sec=sec;
	//req.tv_nsec=milisec*1000000L;
	req.tv_nsec=nanosec;

	//fprintf(stdout, "sleep time %lu \n",nanosec);

	__nsleep(&req,&rem);
	return 1;
}

int memcpy_delay(void *dest, void *src, size_t len) {

	unsigned long lat_ns, cycles;

	lat_ns = calc_delay_ns(len);
	//cycles = NS2CYCLE(lat_ns);		
	//emulate_latency_cycles(cycles);	
	msleep(lat_ns);
	//pthread_mutex_lock(&chkpt_mutex);
	memcpy(dest,src,len);
	//pthread_mutex_unlock(&chkpt_mutex);
	return 0;
}


int memcpy_delay_temp(void *dest, void *src, size_t len) {

	unsigned long lat_ns, cycles;

	lat_ns = calc_delay_ns(len);
	msleep(lat_ns);
	memcpy(dest,src,len);
	return 0;
}

map<std::string,int> mymap;
map<std::string,int>::iterator it;

void hash_insert( std::string key, int val )
{
	mymap[key] = val;
}

void hash_increment(std::string key)
{

	mymap[key]++;
}


int hash_find( std::string key) {

	it =  mymap.find(key );
	if ( it != mymap.end())
		return 0;

	return -1;
}

int same_pages = 0;
int diff_pages = 0;
int hash_clear() {

	mymap.clear();
#ifdef _COMPARE_PAGES
	same_pages = 0;
	diff_pages = 0;
#endif
	return 0;
}


void hash_delete( std::string key) {

	it =  mymap.find(key );
	if ( it != mymap.end())
	{
		mymap.erase (it);

	}
}

size_t find_hash_total() {

	size_t total_val =0;

	for( it =  mymap.begin(); it != mymap.end(); it++){
		total_val += it->second;			
	}

	return total_val;
}

#ifdef _COMPARE_PAGES 


int update_content_hash(void *src_buff) {

	int keylen = 256;
	char gen_key[keylen];
	bzero(gen_key, keylen);
	sha1_mykeygen(src_buff, gen_key, keylen, 16, UNIT_SIZE);
	std::string s(gen_key);
	if(hash_find(s)) {
		hash_insert(s,0);
		return 0;
	}else{
		hash_increment(s);
		return 1;
	}
	return 0;
}


int compare_content_hash( void *src, void *dest, size_t length) {

	size_t cntr = 0;


	while ( (length > UNIT_SIZE) &&  cntr < (length - UNIT_SIZE)) {
		if(!memcmp(src, dest, UNIT_SIZE)){
			same_pages++;
		}else{
			diff_pages++;
		}
		src = src + UNIT_SIZE;
		dest = dest + UNIT_SIZE;
		cntr = cntr + UNIT_SIZE;

	}


	/*	while ( (length > unit_size) &&  cntr < (length - unit_size)) {
	 	update_content_hash(src);
		src = src + unit_size;
		cntr = cntr + unit_size;

	}

	cntr = 0;

	while ( (length > UNIT_SIZE) && cntr < (length- UNIT_SIZE)) {

	 	if(update_content_hash(dest))
		  same_pages++;

		dest = dest + UNIT_SIZE;
		cntr = cntr + UNIT_SIZE;
	}

	 */
	fprintf(stdout,"same_pages:%d diff_pages: %d \n",same_pages, diff_pages);
	return same_pages;

}

#endif // COMPARE_PAGES

map<int,struct BWSTAT*> bw_map;
map<int,struct BWSTAT*>::iterator bw_it;

void add_bw_timestamp( int id, struct timeval st,	
		struct timeval end, size_t bytes )
{

	struct BWSTAT *bwstat = ( struct BWSTAT *)malloc(sizeof(struct BWSTAT));
	bwstat->start_time = st;
	bwstat->end_time = end;
	bwstat->bytes = bytes;
	bw_map[id] = bwstat;
	/*fprintf(stdout, "%d  %ld  %ld  %u\n",
			id,
			st.tv_sec*1000000 + st.tv_usec,
			end.tv_sec*1000000 + end.tv_usec,
			bytes);*/
}


void print_all_bw_timestamp() {

	for( bw_it =  bw_map.begin(); bw_it != bw_map.end(); bw_it++){

		struct BWSTAT *bwstat = (struct BWSTAT *)bw_it->second;

		fprintf(stdout, "%ld  %ld  %zu\n",
				bwstat->start_time.tv_sec*1000000 + bwstat->start_time.tv_usec,
				bwstat->end_time.tv_sec*1000000 + bwstat->end_time.tv_usec,
				bwstat->bytes);
	}
	//fprintf(stdout,"\n\n\n\n");
}

int posix_fileopen(char *base_name, const char *mode){
	FILE *fp = fopen(base_name, mode);
	if (fp) {
	  return fileno(fp);
	}else {
		return -1;
	}
}


void posix_close(int fd){
	if(fd != -1)
	  close(fd);
}


char* generate_file_name(char *base_name, int pid, char *dest) {
	int len = strlen(base_name);
	char c_pid[16];
	sprintf(c_pid, "%d", pid);
	memcpy(dest,base_name, len);
	len++;
	strcat(dest, c_pid);
	return dest;
}


int create_map_file(char *basepath, int pid, char *out_fname,
		int vmaid){

	char fileid_str[64];
	generate_file_name((char *)basepath,pid,out_fname);
	sprintf(fileid_str, "%d", vmaid);
	strcat(out_fname,"_");
	strcat(out_fname, fileid_str);
	return 0;
}


int  setup_map_file(char *filepath, unsigned int bytes)
{
	off64_t result;
	int fd;

	fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0666);
	//fd = posix_fileopen(filepath, "w+");
	if (fd == -1) {
		perror( filepath);
		exit(EXIT_FAILURE);
	}
	assert(!ftruncate(fd,bytes));
	return fd;
}

int  check_existing_map_file(char *filepath)
{
	int fd;

	fd = open(filepath, O_RDWR, (mode_t) 0666);
	if (fd == -1) {
		//perror( filepath);
		return -1;
	}
	return fd;
}


#ifdef _OBJNAMEMAP
//////START - CODE FOR OBJ NAME MAPPING/////////////////////
void objnamemap_insert( char *key, int val )
{

	//printf("objnamemap_insert %s\n",key);
	std::string keystr(key);
	objnamemap[keystr] = val;
}

void objnamemap_increment(char *key)
{

	std::string keystr(key);
	objnamemap[keystr]++;
}


int objnamemap_find(char *key) {

	std::string keystr(key);

	it =  objnamemap.find(keystr );
	if ( it != objnamemap.end())
		return 0;

	return -1;
}

int objnamemap_clear() {

	objnamemap.clear();
	return 0;
}


void objnamemap_delete(char *key) {

	std::string keystr(key);

	//printf("objnamemap_delete %s\n",key);
	it =  objnamemap.find(keystr );
	if ( it != objnamemap.end())
	{
		objnamemap.erase (it);

	}
}

size_t find_objnamemap_total() {

	size_t total_val =0;


	for( objmap_it =  objnamemap.begin();
			objmap_it != objnamemap.end(); objmap_it++){

		total_val += objmap_it->second;
	}

	return total_val;
}

char** get_object_list(int *count){

	char **objnamelist;
	int i=0, len;
	std::string objname;


	objnamelist = (char **)malloc(sizeof(char *) * objnamemap.size());
	assert(objnamelist);

	for (i = 0, objmap_it =  objnamemap.begin();
			i < objnamemap.size(), objmap_it != objnamemap.end();
			i++, objmap_it++){

		objnamelist[i] = (char *)malloc(sizeof(char)* MAXOBJLEN);
		assert(objnamelist[i]);

		objname = (std::string) objmap_it->first;
		len = strlen(objname.c_str());
		if(len){
			//fprintf("get_object_list %s \n", objname.c_str());
			memcpy(objnamelist[i], objname.c_str(),len);
			objnamelist[i][len]=0;
		}
	}
	*count = objnamemap.size();

	return objnamelist;
}
//////END -CODE FOR OBJ NAME MAPPING/////////////////////
#endif





