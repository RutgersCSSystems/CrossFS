/*
 * nv_transact.h
 *
 *  Created on: Mar 19, 2013
 *      Author: sudarsun@gatech.edu
 */

#ifndef NV_TRANSACT_H_
#define NV_TRANSACT_H_


#ifdef __cplusplus
extern "C" {
#endif

////////////////////TRANSACTION RELATED FUNCTIONS///////////////////
int nv_commit(rqst_s *rqst);
/*uses object pointer to commit*/
int nv_commit_obj(void* objptr);
/*when commit size is less than cache line size*/
int nv_commit_word(void* wordptr);
/*uses object pointer to start a transaction*/
int nv_begintrans_obj(void* objptr);
/*word based transactions*/
int nv_begintrans_wrd(void* wrdptr, size_t size);
int LogWord(void *addr, size_t size);
void print_trans_stats(void);
////////////////////REDO RELATED FUNCTIONS///////////////////
int store_log(nvword_t *addr, nvword_t value);
nvword_t load_log(nvword_t *addr);
int nvcommit_noarg(void);
////////////////////FLUSH/SYNC RELATED FUNCTIONS///////////////////
int nv_sync_obj(void *objptr);
int nv_sync_chunk(void *objptr, size_t len);
int nv_initialize_log(void *addr);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif /* NV_TRANSACT_H_ */
