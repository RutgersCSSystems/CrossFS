/*
 * nv_debug.h
 *
 *  Created on: Apr 16, 2013
 *      Author: hendrix
 */

#ifndef NV_DEBUG_H_
#define NV_DEBUG_H_


#include "nv_map.h"

void DEBUG_PROCOBJ(proc_s *proc_obj);
void DEBUG_MMAPOBJ(mmapobj_s *mmapobj);
void DEBUG_MMAPOBJ_T(mmapobj_s *mmapobj);
void DEBUG_CHUNKOBJ(chunkobj_s *chunkobj);
void DEBUG_CHUNKOBJ_T(chunkobj_s *chunkobj);
void DEBUG(const char* format, ... );
void DEBUG_T(const char* format, ... );

#endif /* NV_DEBUG_H_ */
