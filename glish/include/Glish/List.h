// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef glish_list_h_
#define glish_list_h_

#include <Glish/Object.h>

//
// List.h was moved to the sos library to prserve library
// dependencies. This header file remains for historical
// reasons.
//

inline void *int_to_void(int i)
	{
	void *ret = 0;
	*((int*)&ret) = i;
	return ret;
	}

inline int void_to_int(void *v)
	{
	return *(int*)&v;
	}

#include <sos/list.h>

// Popular type of list: list of strings.
declare(PList,char);
typedef PList(char) name_list;

#endif /* glish_list_h_ */
