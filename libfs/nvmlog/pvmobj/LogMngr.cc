
//enable if code needs to be compiled in standalong
//debugging mode
//#define _STANDALONE

#include <stdint.h>
#include "nv_map.h"
#include "nv_def.h"
#include "util_func.h"
//#include "db_impl.h"

#include <iostream>
#include <string>
#include <unordered_map>

#define MAX_NESTED_TRANS 256


uint8_t use_wal;

uint8_t loginitalized;
//Global variables per application
void *objrecmap = NULL;
void *objdatmap = NULL;

void *wrdrecmap = NULL;
void *wrddatmap = NULL;

record_s *currobjrec = NULL;
void *curdatobjptr = NULL;

wrdrec_s *currwrdrec = NULL;
void *curdatwrdptr = NULL;
void *wrdbitmap = NULL;
UINT wrdbitmapcnt;
UINT objdataoff;
UINT objheaderoff;
UINT g_recid;
UINT wrddataoff;
UINT wrdheaderoff;


ULONG wrd_rechead[MAX_NESTED_TRANS];
ULONG wrd_datahead[MAX_NESTED_TRANS];
UINT wrd_nstd_cntr;

ULONG obj_rechead[MAX_NESTED_TRANS];
ULONG obj_datahead[MAX_NESTED_TRANS];
UINT obj_nstd_cntr;

using namespace std;

std::unordered_map <ULONG,wrdrec_s *> wrdloghash;


#ifdef _NVDEBUG
static unsigned int stats=0;
#endif


////////METHODS TO INITIALIZE WORD AND OBJ MANAGERS/////////////////////
int init_obj_logmngr(int pid, int isnewLog){


	currobjrec = (record_s *)objrecmap;
	curdatobjptr = (void *)objdatmap;

	objdataoff = 0;
	objheaderoff = 0;
	obj_nstd_cntr =0;
	//append a master record
	if(isnewLog) {
		add_master_record((void *)currobjrec);
	}

	currobjrec += sizeof(master_s);
	//This will be the recordmap start position
	objrecmap = (void *)currobjrec;
	//update the offset accordingly
	objheaderoff += sizeof(master_s); 

}
int init_wrd_logmngr(int pid, int isnewLog){


	void *temp = NULL;

	wrd_nstd_cntr =0;
	

	currwrdrec = (wrdrec_s *)wrdrecmap;
	curdatwrdptr = (void *)wrddatmap;
	
	//initialize the offsets
	wrddataoff = 0;
	wrdheaderoff = 0;

	//append a master record
	if(isnewLog) {
		add_master_record((void *)currwrdrec);
		master_s *newrec;
		newrec = (master_s *)wrdrecmap;
	}else {
		 master_s *newrec;
		 newrec = (master_s *)wrdrecmap;
		 fprintf(stdout,"Master rec%u \n",newrec->reccount);
	}

	temp = (void *)currwrdrec;
	temp = (void *)((ULONG)temp + sizeof(master_s));

	currwrdrec = (wrdrec_s *)temp;
	wrdrecmap = (void *)currwrdrec;
	wrdheaderoff += sizeof(master_s); 
}

int get_file(const char* name,int pid, 
		int isnewLog, UINT vma_id,
		UINT size){

	char filename[256];
	bzero(filename, 256);
	int fd = -1;

	create_map_file((char *)name, pid, filename,vma_id);

	if(isnewLog){
		fd = setup_map_file(filename,size+1);
	}
	else{
		fprintf(stderr,"openning file %s\n",filename);
		FILE *fp = fopen(filename,"rw+");
		fd = fileno(fp);
	}
	assert(fd >= 0);
	return fd;
}


int map_log_records(int pid, int isnewLog) {


	int fdobj = -1, fdobj1= -1;
	int fdwrd = -1, fdwrd1= -1;
	nvarg_s s;

	s.proc_id = pid;
	s.vma_id = TRANS_LOGVMAID;

#ifdef _STANDALONE
	fdobj = get_file((char*)PROCLOG_PATH,pid,isnewLog,s.vma_id,TRANS_LOGSZ);
	assert(fdobj >= 0);
	fdwrd = get_file((char*)PROCLOG_PATH,pid,isnewLog,s.vma_id+1,TRANS_LOGSZ);
	assert(fdwrd >= 0);

	s.proc_id = pid;
	s.vma_id = TRANS_DATA_LOGVMAID;
	fdobj1 = get_file((char*)PROCLOG_DATA_PATH,pid,isnewLog,s.vma_id+1, TRANS_DATA_LOGSZ);
	assert(fdobj1 >= 0);
	fdwrd1 = get_file((char*)PROCLOG_DATA_PATH,pid,isnewLog,s.vma_id+1, TRANS_DATA_LOGSZ);
	assert(fdwrd1 >= 0);

	wrdrecmap = mmap(0, TRANS_LOGSZ, PROT_NV_RW, MAP_SHARED, fdwrd, 0);
	wrddatmap = mmap(0,TRANS_DATA_LOGSZ, PROT_NV_RW, MAP_SHARED, fdwrd1, 0);

	objrecmap = mmap(0, TRANS_LOGSZ, PROT_NV_RW, MAP_SHARED, fdobj, 0);
	objdatmap = mmap(0,TRANS_DATA_LOGSZ, PROT_NV_RW, MAP_SHARED, fdobj1, 0);
#else
	objrecmap = mmap(0, TRANS_LOGSZ, PROT_NV_RW, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
	objdatmap = mmap(0, TRANS_DATA_LOGSZ, PROT_NV_RW, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);

	wrdrecmap = mmap(0, TRANS_LOGSZ, PROT_NV_RW, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
	wrddatmap = mmap(0, TRANS_DATA_LOGSZ, PROT_NV_RW, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);
#endif
	assert(wrdrecmap != MAP_FAILED);
	assert(wrddatmap != MAP_FAILED);
	assert(objrecmap != MAP_FAILED);
	assert(objdatmap != MAP_FAILED);

}


/*Initialize leveldb log manager*/
int initialize_logmgr(int pid, int isnewLog){

	loginitalized=1;

#ifdef _USE_REDO_LOG
	use_wal = 1;
#endif
	//set the global recordid to 0
	g_recid = 0;
	map_log_records(pid,isnewLog);
	//initialize log and word based managers
	init_obj_logmngr(pid,isnewLog);
	init_wrd_logmngr(pid,isnewLog);
	//wrdbitmap = (void *)currwrdrec;
	//wrdbitmapcnt = 0;
	//currwrdrec +=create_loguse_bitmap((void *)currwrdrec, TRANS_LOGSZ);
	return 0;
}
//////////////////////////////////////////////////////////////////////////////



int add_master_record(void *logrec) {

	master_s *newrec;

	newrec = (master_s *)logrec;
	newrec->reccount =0;
	return 0;
}


int create_loguse_bitmap(void *logrec, UINT logsize) {

	UINT bitmapsz = logsize/8;
	int idx =0;
	unsigned char *ptr;

	ptr = (unsigned char *)logrec;
	for(idx=0; idx < bitmapsz; idx++){
		*ptr =0;
		ptr++;
	}
	return bitmapsz;
}

int set_loguse_bitmap(void *bitmap_ptr, UINT counter) {

	//Each bit is equal to
	unsigned char *ptr;
	UINT bytepos =0, byteoff =0;
	uint8_t mask=1;

	bytepos = counter/8;
	byteoff = counter % 8;
	ptr = (unsigned char *)bitmap_ptr;
	ptr = (unsigned char *)((ULONG)ptr +bytepos);

	if(byteoff){
		mask = mask <<byteoff;
	}
	*ptr = *ptr|mask;
	return *ptr;
}

int compare_logbitmap_val(void *bitmap_ptr, UINT value) {

}

int check_commit_bits(unsigned char *ptr, UINT len){

	int idx = 0;

	for(idx=0; idx < len; idx = idx+8){
		if(*ptr != 0)
			return 0;
		ptr+= 8;
	}
	return 1;
}

int scavenge_loguse_bitmap(void *bitmap_ptr, size_t logsize) {

	UINT bitmapsz = logsize/8;
	int idx =0;
	unsigned char *ptr = (unsigned char *)bitmap_ptr;


	for(idx=0; idx < bitmapsz; idx=idx+8){

		if(!check_commit_bits(ptr, 8)){
			return -1;
		}
		ptr++;
	}
	return 0;
}


int print_byte_in_binary(uint8_t val){

	while (val) {
		if (val & 1)
			fprintf(stdout,"1");
		else
			fprintf(stdout,"0");
		val >>= 1;
	}
}

int print_loguse_bitmap(void *bitmap_ptr, UINT counter) {

	//Each bit is equal to
	UINT idx=0;
	unsigned char *ptr;
	UINT bytepos =0, byteoff =0;
	UINT numbytes = counter/8 + 1;

	ptr = (unsigned char *)bitmap_ptr;
	for (idx=0; idx < numbytes; idx++){
		print_byte_in_binary(*ptr);
		fprintf(stdout,"\t");
		ptr++;
	}
	printf("\n");
	return *ptr;
}

int get_rec_count(void *baselogrecptr){

	master_s *master;
	void *ptr = (void *)((ULONG)baselogrecptr - sizeof(master_s));
	master = (master_s *)ptr;
	return master->reccount;
}

void incr_rec_count(void *baselogrecptr){

	master_s *master;
	void *ptr = (void *)((ULONG)baselogrecptr - sizeof(master_s));
	master = (master_s *)ptr;
	master->reccount++;
}

int add_data_log(void *dateptr,void *logdataptr, UINT length,  UINT chunkid){

	assert(dateptr);
	assert(logdataptr);
	memcpy(logdataptr, dateptr, length);
	flush_cache(logdataptr, length);
}


int add_obj_record(void *dateptr, UINT length,  UINT chunkid){

	record_s *prev_rec;
	size_t incr =0;
	void *tmp;

	assert(currobjrec);

	incr = sizeof(record_s);

	if((ULONG)currobjrec >= (ULONG)((ULONG)objrecmap + TRANS_LOGSZ)){
		assert(0);
	}
	if((ULONG)curdatobjptr >= (ULONG)((ULONG)objdatmap + TRANS_DATA_LOGSZ)){
		assert(0);
	}
	add_data_log(dateptr, curdatobjptr, length,  chunkid);


	//fprintf(stdout,"Incrementing objdataoff %u "
	//		"by %u \n",objdataoff, length);
	currobjrec->length=length;
	currobjrec->offset =objdataoff;
	objdataoff += length;
	curdatobjptr = (void *)((ULONG)curdatobjptr + length);

	tmp = (void *)currobjrec;
    tmp = (void *)((ULONG)tmp + incr);
	currobjrec =(record_s*)tmp;

	incr_rec_count(objrecmap);
	objheaderoff+= sizeof(record_s);

#if 0
	currobjrec->addrval=(ULONG)dateptr;
	currobjrec->recid =++g_recid;
	currobjrec->nxt = NULL;
	if (prev_rec) {
		prev_rec->nxt = currobjrec;
	#ifdef _NVDEBUG
		fprintf(stdout,"assigning prev_rec->recid "
				"%u to curr->recid %u \n",
				 prev_rec->recid, currobjrec->recid);
	#endif
	}
#endif
}

int add_word_record(void *dateptr){

	//wrdrec_s *prev_rec;
	UINT length =0;

	assert(currwrdrec);
	length = CACHE_LINE_SIZE;

	if((ULONG)currwrdrec >= (ULONG)((ULONG)wrdrecmap + TRANS_LOGSZ)){
		//assert(0);
		currwrdrec =(wrdrec_s*)wrdrecmap;
		wrdheaderoff = 0;
	}
	if((ULONG)curdatwrdptr >= (ULONG)((ULONG)wrddatmap + TRANS_DATA_LOGSZ)){
		//assert(0);
		curdatwrdptr = wrddatmap;
		wrddataoff = 0;
	}
	add_data_log(dateptr, curdatwrdptr, length, 0);
	currwrdrec->dataoff =wrddataoff;
	wrddataoff += length;
	curdatwrdptr = (void *)((ULONG)curdatwrdptr + length);
	currwrdrec = (wrdrec_s*)((ULONG)currwrdrec + sizeof(wrdrec_s));
	wrdheaderoff+= sizeof(wrdrec_s);

	/*increments master record count*/
	incr_rec_count(wrdrecmap);


	/*if it is a write ahead log,then hash words*/
	if(use_wal){
		add_wrd_hash(dateptr,currwrdrec);
	}
}

UINT reduce_offset(ULONG curr, ULONG prev){

	return curr > prev ? (curr- prev): (prev-curr);
}

//when a transaction begins
//the head
int start_wrd_trans(void *dataptr){

	wrd_rechead[wrd_nstd_cntr] = (ULONG)currwrdrec;
	wrd_datahead[wrd_nstd_cntr] =(ULONG)curdatwrdptr;
	wrd_nstd_cntr++;

}

//when a transaction is complete
//the log must be set to same original
//location
int end_wrd_trans(void *dataptr){

	UINT idx = 0; 

	if(wrd_nstd_cntr)
	wrd_nstd_cntr--;

	idx = wrd_nstd_cntr;
	wrdheaderoff= wrdheaderoff - reduce_offset((ULONG)currwrdrec, 
					(ULONG)wrd_rechead[idx]);

    wrddataoff = wrddataoff -  reduce_offset((ULONG)curdatwrdptr,  
            	  (ULONG)wrd_datahead[idx]);          

	currwrdrec = (wrdrec_s *)wrd_rechead[idx];
	curdatwrdptr = (void *)wrd_datahead[idx];

	/*if it is a write ahead log,then hash words*/
	if(use_wal){
		rmv_wrd_hash(dataptr);
	}

	//fprintf(stdout,"offset %lu %lu\n", wrddataoff, wrdheaderoff);
}

//when a transaction begins
//the head for obj log is set
int start_obj_trans(void *dataptr){

	obj_rechead[obj_nstd_cntr] = (ULONG)currobjrec;
	obj_datahead[obj_nstd_cntr] = (ULONG)curdatobjptr;
	obj_nstd_cntr++;

}

//when a transaction is complete
//the log must be set to same original
//location
int end_obj_trans(void *addr){

	UINT idx = 0; 

    if(obj_nstd_cntr) {
		obj_nstd_cntr--;
	}

	idx = obj_nstd_cntr;
	objheaderoff =reduce_offset((ULONG)currobjrec,  
                (ULONG)obj_rechead[idx]);
	#if 0
	fprintf(stdout,"dec. objdataoff %u by %u \n",
					reduce_offset((ULONG)curdatobjptr,
		     		(ULONG)obj_datahead[idx]));
	#endif
	objdataoff =objdataoff - reduce_offset((ULONG)curdatobjptr,  
                (ULONG)obj_datahead[idx]);
	currobjrec = (record_s *)obj_rechead[idx];
	curdatobjptr = (void *)obj_datahead[idx];

	//fprintf(stdout,"objdataoff %u \n",objdataoff);
}


int log_record(void *dateptr, UINT length,  UINT chunkid, uint8_t isWrdLogging){

	if(!isWrdLogging){
		start_obj_trans(dateptr);
		add_obj_record(dateptr, length, chunkid);
	}else{
		//record the log pointers
		//before start of transaction
		//we use global record and data
		//pointers, so no args are req.
		start_wrd_trans(dateptr);
		add_word_record(dateptr);
	}
	return 0;
}

int truncate_log_commit(void *dateptr,uint8_t isWrdLogging){

	if(!isWrdLogging){

		end_obj_trans(dateptr);
	}else{
		//revert back the log position to a
		//location before transaction.
		//Note: nested transaction must maintain
		//order
		end_wrd_trans(dateptr);
	}
	return 0;
}


int update_oncommit(void *dateptr,uint8_t isWrdLogging){

	if(!isWrdLogging){
		truncate_log_commit(dateptr, isWrdLogging);
	}else{
		truncate_log_commit(dateptr, isWrdLogging);
	}
	return 0;
}




void print_log_stats(void){

	if(loginitalized){
		fprintf(stdout,"obj records: %u\n",get_rec_count(objrecmap));
		fprintf(stdout,"obj log header size: %u\n",objheaderoff);
		fprintf(stdout,"obj log data size: %u\n",objdataoff);

		fprintf(stdout,"word records: %u\n",get_rec_count(wrdrecmap));
		fprintf(stdout,"word log header size: %u\n",wrdheaderoff);
		fprintf(stdout,"word log data size: %u\n",wrddataoff);
		fprintf(stdout,"totla log data size: %u\n",
				wrdheaderoff+ wrddataoff+objheaderoff+objdataoff);
	}else{
		fprintf(stdout,"log not initialized \n");
	}
}




///////////////UNDO COMMIT DATASTRUCTURES////////////////////////////////////////

int add_redo_word_record(nvword_t *dateptr, nvword_t value){

        //wrdrec_s *prev_rec;
        UINT length =0;

        assert(currwrdrec);
        length = sizeof(nvword_t);

        if((ULONG)currwrdrec >= (ULONG)((ULONG)wrdrecmap + TRANS_LOGSZ)){
                //assert(0);
                currwrdrec =(wrdrec_s*)wrdrecmap;
                wrdheaderoff = 0;
        }
        if((ULONG)curdatwrdptr >= (ULONG)((ULONG)wrddatmap + TRANS_DATA_LOGSZ)){
                //assert(0);
                curdatwrdptr = wrddatmap;
                wrddataoff = 0;
        }

        add_data_log(&value, curdatwrdptr, length, 0);
        currwrdrec->dataoff =wrddataoff;
		add_wrd_hash(dateptr,currwrdrec);


		//increment all for subsequent iteration
        wrddataoff += length;
		wrdheaderoff+= sizeof(wrdrec_s);
        curdatwrdptr = (void *)((ULONG)curdatwrdptr + length);
        currwrdrec = (wrdrec_s*)((ULONG)currwrdrec + sizeof(wrdrec_s));

        
        /*increments master record count*/
        incr_rec_count(wrdrecmap);

		return 0;
}

//Function to read the data from log
//First lookup addr index to find word
//record. then get the data
nvword_t read_log(nvword_t *ptr) {

	wrdrec_s *wordrec;	
	void *addr = NULL;
	nvword_t *wrd;


	//you can't send me a null
    //pointer.		
	assert(ptr);

	wordrec = (wrdrec_s *)find_wrd_hash(ptr);

	if(!wordrec) {
		//assert(0);
		//return 0;
		//fprintf(stdout,"not in log, using mem val\n");
		return *ptr;
	}

	//fprintf(stdout,"wordrec->dataoff %u\n",wordrec->dataoff);

	addr = (void *)((ULONG)wrddatmap + wordrec->dataoff);
	wrd = (nvword_t *)addr;

	if(!wrd) return 0;

	return *wrd;
}

void add_wrd_hash(void *ptr, wrdrec_s *wr){

	wrdloghash[(ULONG)ptr]=wr;
}

void rmv_wrd_hash(void *ptr){

	wrdloghash.erase((ULONG)ptr);
}

wrdrec_s* find_wrd_hash(void *ptr){

	unordered_map<ULONG,wrdrec_s *>::iterator itr;

	itr = wrdloghash.find((ULONG)ptr);

	if ( itr == wrdloghash.end() ) {
		return NULL;
	}
	else {
    	 return	 itr->second;
	}
}

//Function to clear data after commit
nvword_t clear_data(wrdrec_s *wordrec) {

	void *addr = NULL;
	nvword_t *wrd;

	addr = (void *)((ULONG)wrddatmap + wordrec->dataoff);
	wrd = (nvword_t *)addr;
	memset(addr, 0, sizeof(nvword_t));
	return *wrd;
}

void clear_wrdrec ( void *buff) {

	memset(buff,0,sizeof(wrdrec_s));
}

int commit_all(void) {

	unordered_map<ULONG,wrdrec_s *>::iterator itr;
	wrdrec_s *wordrec = NULL;
	nvword_t *first = NULL;
	nvword_t value;
    void *buff = NULL;
	unsigned int max = 0;

    for( itr= wrdloghash.begin(); itr!=wrdloghash.end(); ++itr){

        first = (nvword_t *)(*itr).first;
		assert(first);

		wordrec = (wrdrec_s *)(*itr).second;
		assert(wordrec);
		value = read_log(first);

		////BUGGY AREA: TEMPORARY FIX FOR READING CORRECT LOG SIZE
       //FIXME: BUG:copy data to log
       if(value > (~max)) {
           *first = value;
       }
       else {
			//copy data to log
			memcpy((void *)first,&value,sizeof(unsigned int));
    	}

		rmv_wrd_hash(first);

		//clear the data from log
		clear_data(wordrec);

		//clear the wordrec from log
		buff = (void *)wordrec;
		clear_wrdrec(buff);
    }   
}

///////////////REAO COMMIT DATASTRUCTURES////////////////////////////////////////

void print_obj_rec(record_s *rec){

	fprintf(stdout,"rec->length: %u \n",rec->length);
	fprintf(stdout,"rec->datalogoffset: %u \n",rec->offset);
	//fprintf(stdout,"rec->addr: %lu \n",rec->addrval);
	//fprintf(stdout,"rec->recid: %u \n",rec->recid);
}
void print_obj_rec_data(record_s *rec){

	char *base = (char *)objdatmap;
	char *data = base + rec->offset;
	fprintf(stdout,"data: %s \n",(char *)data);
}

void print_wrd_rec(wrdrec_s *rec){

	//fprintf(stdout,"rec->datalogoffset: %u \n",rec->dataoff);

}
void print_wrd_rec_data(wrdrec_s *rec){

	char *base = (char *)objdatmap;
	char *data = base + rec->dataoff;
	
	nvword_t *word = (nvword_t *)data;
	//fprintf(stdout,"data: %ld\n", *word);
}

int debug_log(void *reclogptr,void *datalogptr, UINT record_cnt, UINT isWordLogging){

	record_s *rec;
	wrdrec_s *wrdrec;
	UINT count =0;

	if(!isWordLogging) {
		rec =  (record_s *)reclogptr;
		while(rec != NULL && count < record_cnt) {
			print_obj_rec(rec);
			print_obj_rec_data(rec);
			rec = (record_s *)((ULONG)rec + sizeof(record_s));
			count++;
		}
	}else {

		wrdrec =  (wrdrec_s *)reclogptr;

		while(wrdrec != NULL && count < record_cnt) {
			print_wrd_rec(wrdrec);
			print_wrd_rec_data(wrdrec);
			wrdrec = (wrdrec_s *)((ULONG)wrdrec + sizeof(wrdrec_s));
			count++;
		}
	}

}

#ifdef _STANDALONE
int main(int argc, char *argv[]){

	UINT record_count = 0, idx =0;
	void **chunks = NULL;
	uint8_t restart =0, isWordLogging=0;	

	if(argc < 4){
		fprintf(stdout,"incorrect args "
				"arg 1: if write new log or read log \n"
				"arg 2: number of log enteries for testingn"
				"arg 3: is word logging \n");
		exit(-1);
	}


	record_count = atoi(argv[2]);
	restart = atoi(argv[1]);
	isWordLogging = atoi(argv[3]);


	/*if new log then 1 else if restart then 0*/
	/*6666 some pid*/
	assert(!initialize_logmgr(6666, !restart));

	if(!restart) {
		chunks = (void **)malloc(record_count* sizeof(void *));
		assert(chunks);

		for(idx = 0; idx < record_count; idx++){
			chunks[idx] = (void *)malloc(CACHELINESZ);
			assert(chunks[idx]);
			sprintf((char *)chunks[idx],"%u",idx);

			 nvword_t value = (nvword_t)idx;

			if(use_wal) {
				//use word based logging
				add_redo_word_record((nvword_t *)chunks[idx], value);
			}else {
				//use obj based logging
				log_record(chunks[idx], CACHELINESZ,  idx, isWordLogging);
			}

		}
		if(use_wal) {

			int i=0;
			//Test redo log to read values from hash
			for(i = 0; i < record_count; i++){
				fprintf(stdout, "%ld \n",read_log((nvword_t *)chunks[i]));
			}	
		}
	}else {

		if(isWordLogging){
			record_count = get_rec_count(wrdrecmap);
		}
		else {
			record_count = get_rec_count(objrecmap);
		}
		assert(record_count);

		if(isWordLogging) {
			debug_log(wrdrecmap,wrddatmap, record_count, isWordLogging);
		}else {
			debug_log(objrecmap,objdatmap, record_count, isWordLogging);
		}
	}

	msync((void *)((ULONG)objrecmap - sizeof(master_s)), TRANS_LOGSZ, MS_SYNC);
	msync(objdatmap, TRANS_LOGSZ, MS_SYNC);

	exit(0);


}
#endif

