// $Header$

// Copyright (c) 1993 The Regents of the University of California.
// All rights reserved.
//
// This code is derived from software contributed to Berkeley by
// Vern Paxson.
//
// The United States Government has rights in this work pursuant
// to contract no. DE-AC03-76SF00098 between the United States
// Department of Energy and the University of California, and
// contract no. DE-AC02-89ER40486 between the United States
// Department of Energy and the Universities Research Association, Inc.
//
// Redistribution and use in source and binary forms are permitted
// provided that: (1) source distributions retain this entire
// copyright notice and comment, and (2) distributions including
// binaries display the following acknowledgement:  ``This product
// includes software developed by the University of California,
// Berkeley and its contributors'' in the documentation or other
// materials provided with the distribution and in all advertising
// materials mentioning features or use of this software.  Neither the
// name of the University nor the names of its contributors may be
// used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE.

#include <stdio.h>

#include "Reporter.h"
#include "Sequencer.h"

static Sequencer* s;

int main( int argc, char** argv )
	{
	s = new Sequencer( argc, argv );

	s->Exec();

	// We don't delete s because presently the Sequencer class does
	// not reclaim its sundry memory upon deletion, so Purify complains
	// about zillions of memory leaks.  This is also the reason why
	// s is a static and not a local.

	return 0;
	}
