/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file alignedops.cpp
 *
 *  @brief Implements aligned memory operations.
 *
 *  @author Naju Mancheril (ngm)
 */
#include "util/alignedops.h" /* for prototypes */
#include "util/guard.h"        /* for guard*/
#include "util/trace.h"        /* for TRACE */
#include <cstring>
#include <cassert>



/* debugging */

static const int debug_trace_type = TRACE_DEBUG;
#define DEBUG_TRACE(format, arg) TRACE(debug_trace_type, format, arg)



/* definitions of exported functions */

/**
 *  @brief Returned an aligned copy of the data.
 *
 *  @return NULL on error.
 */
void* aligned_alloc(void*  buf,
                    size_t buf_size, size_t align_size,
                    void** aligned_base) {
    
    assert(buf_size > 0);
    assert(align_size > 0);

    /* allocate space */
    size_t alloc_size = buf_size + align_size - 1;
    guard<char> big_buf = new char[alloc_size];
    if (big_buf == NULL)
        return NULL;

    /* locate aligned location */
    size_t aligned_base_addr;
    for (aligned_base_addr = (size_t)(void*)big_buf;
         (aligned_base_addr % align_size) != 0; aligned_base_addr++);
    
    void* dst = (void*)aligned_base_addr;
    assert(aligned_base_addr < ((size_t)(void*)big_buf + alloc_size));
    *aligned_base = dst;
    
    return big_buf.release();
}
