// $Header$

#ifndef tkcanvas_h
#define tkcanvas_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"
#include "TkAgent.h"

class TkCanvas : public TkAgent {
    public:
	TkCanvas( Sequencer *, TkFrame *, charptr relief, charptr width, charptr height,
		  charptr borderwidth, charptr background );

	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkCanvas();
	};

#endif
#endif
