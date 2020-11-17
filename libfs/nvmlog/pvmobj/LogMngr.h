#ifndef LOGMNGR_H_
#define LOGMNGR_H_

#include "nv_def.h"
#include <stdint.h>

struct record{
	//UINT recid;
	ULONG virtaddr;
	UINT length;
	UINT offset;
	//ULONG addrval;
	//struct record *nxt;
};
typedef struct record record_s;

struct master_rec{
	UINT reccount;
	ULONG baseaddr;
	struct master_rec *nxt;
};
typedef struct master_rec master_s;

struct wrdrecord{
	//based addr+offset will
	//give virtual addr
	ULONG virtaddr;
	UINT dataoff;
};
typedef struct wrdrecord wrdrec_s;


/*Initialize leveldb log manager*/
int initialize_logmgr(int pid, int isnewLog);
int log_record(void *dateptr, UINT length,  UINT chunkid, uint8_t isWrdLogging);
int update_oncommit(void *dateptr,uint8_t isWrdLogging);
int iter_record(record_s *record);
void print_record(record_s *rec);
void compute_hash(record_s *rec);
void print_log_stats(void);
int add_master_record(void *logrec);
int truncate_log_commit(uint8_t isWrdLogging);


/*redo log functions*/
void add_wrd_hash(void *ptr, wrdrec_s *wr);
void rmv_wrd_hash(void *ptr);
wrdrec_s* find_wrd_hash(void *ptr);
nvword_t read_log(nvword_t *ptr);
int add_redo_word_record(nvword_t *dateptr, nvword_t value);
int commit_all(void);
#endif
