// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//

#include "config.h"
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Glish/Str.h"
#include "system.h"

StrKernel::~StrKernel()
	{
	if ( str ) free_memory( str );
	}
