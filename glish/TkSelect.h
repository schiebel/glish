// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//
#ifndef tkselect_h
#define tkselect_h

#ifdef GLISHTK

#include "Select.h"
class Sequencer;

class TkSelector : public Selector {
    public:
	TkSelector( Sequencer * );
	~TkSelector();

	void AddSelectee( Selectee* s );
	void DeleteSelectee( int selectee_fd );

	// If selection stops early due to non-zero return from Selectee's
	// NotifyOfSelection(), returns that non-zero value.  Otherwise
	// returns 0.
	int DoSelection( int );
    protected:
	Sequencer *s;
	};

#endif
#endif
