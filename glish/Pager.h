// $Id$
// Copyright (c) 1997 Associated Universities Inc.
#ifndef pager_h_
#define pager_h_

#include "Sequencer.h"
#include "Reporter.h"
#include "Glish/Stream.h"

class PagerReporter : public Reporter {
    public:
	PagerReporter(Sequencer *s) : Reporter( new SOStream ), seq(s) { }

	virtual void report( const ioOpt &opt, const RMessage&,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage 
			   );
    protected:
	void Prolog( const ioOpt & );
	void Epilog( const ioOpt & );
	Sequencer *seq;
};

extern Reporter* pager;
extern void init_interp_reporters( Sequencer * );
extern void finalize_interp_reporters();

#endif
