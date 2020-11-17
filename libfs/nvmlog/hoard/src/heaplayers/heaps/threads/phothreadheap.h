/* -*- C++ -*- */

#ifndef HL_PHOThreadHeap_H_
#define HL_PHOThreadHeap_H_

#include <assert.h>

#include "cpuinfo.h"
using namespace HL;

template <int NumHeaps, class SuperHeap>
class MarkThreadHeap : public SuperHeap {
public:

  inline void * malloc (size_t sz) {
	fprintf(stdout,"phothreadheap.h: CALLING MALLOC %zu \n",sz);
    int tid = CPUInfo::getThreadId() % NumHeaps;
    void * ptr = SuperHeap::malloc (sz);
    if (ptr != NULL) {
      SuperHeap::setHeap(ptr, tid);
      SuperHeap::setPrevHeap(SuperHeap::getNext(ptr), tid);
    }
    return ptr;
  }
};


template <int NumHeaps, class SuperHeap>
class CheckThreadHeap : public SuperHeap {
public:

  inline void * malloc (size_t sz) {
    void * ptr = SuperHeap::malloc (sz);
    if (ptr != NULL) {
      int tid = CPUInfo::getThreadId() % NumHeaps;
      tid;
      assert (SuperHeap::getHeap(ptr) == tid);
    }
    return ptr;
  }

  inline void free (void * ptr) {
    SuperHeap::free (ptr);
  }
};



/*

A PHOThreadHeap comprises NumHeaps "per-thread" heaps.

To pick a per-thread heap, the current thread id is hashed (mod NumHeaps).

malloc gets memory from its hashed per-thread heap.
free returns memory to its originating heap.

NB: We assume that the thread heaps are 'locked' as needed.  */


template <int NumHeaps, class SuperHeap>
class PHOThreadHeap { // : public MarkThreadHeap<NumHeaps, SuperHeap> {
public:

  inline void * malloc (size_t sz) {
    int tid = CPUInfo::getThreadId() % NumHeaps;
    void * ptr = selectHeap(tid)->malloc (sz);
    return ptr;
  }

  inline void free (void * ptr) {
    int tid = SuperHeap::getHeap(ptr);
    selectHeap(tid)->free (ptr);
  }


  inline int remove (void * ptr);
#if 0
  {
    int tid = SuperHeap::getHeap(ptr);
    selectHeap(tid)->remove (ptr);
  }
#endif

private:

  // Access the given heap within the buffer.
  MarkThreadHeap<NumHeaps, SuperHeap> * selectHeap (int index) {
    assert (index >= 0);
    assert (index < NumHeaps);
    return &ptHeaps[index];
  }

  MarkThreadHeap<NumHeaps, SuperHeap> ptHeaps[NumHeaps];

};

#endif
