/* ======================================================================
** sos/alloc.h
**
** $Id$
**
** Copyright (c) 1997 Associated Universities Inc.
**
** ======================================================================
*/
#ifndef sos_alloc_h_
#define sos_alloc_h_

#ifdef __cplusplus
extern "C" {
#endif
void *sos_alloc_memory( unsigned int size );
void *sos_alloc_zero_memory( unsigned int size );
void *sos_realloc_memory( void* ptr, unsigned int new_size );
void sos_free_memory( void* ptr );
#ifdef __cplusplus
	}
#endif

#endif
