// $Header$
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "TkSelect.h"

#ifdef GLISHTK
#include "Sequencer.h"
#include "Rivet/tk.h"

static int selector_status = 0;
static int glish_select = 0;

void glish_event_posted()
	{
	glish_select = 1;
	selector_status = 1;
	}

void TkFileProc ( ClientData data, int mask )
	{
	glish_select = 1;
	selector_status = ((TkSelector*)data)->Selector::DoSelection( 0 );
	}

void TkTimerProc ( ClientData data )
	{
	glish_select = 1;
	selector_status = ((TkSelector*)data)->Selector::DoSelection( 0 );
	}

TkSelector::TkSelector(Sequencer *s_) : Selector(), s(s_) { }

TkSelector::~TkSelector() { }

void TkSelector::AddSelectee( Selectee *s )
	{
	Selector::AddSelectee( s );
	Tk_CreateFileHandler( s->FD(), TK_READABLE, TkFileProc, (ClientData) this );
	}

void TkSelector::DeleteSelectee( int selectee_fd )
	{
	Tk_DeleteFileHandler( selectee_fd );
	Selector::DeleteSelectee( selectee_fd );
	}

int TkSelector::DoSelection( int )
	{
	struct timeval min_t;
	struct timeval timeout_buf;
	struct timeval *timeout = &timeout_buf;

	glish_select = 0;
	FindTimerDelta( timeout, min_t );

	if ( timeout && (timeout->tv_sec || timeout->tv_usec) )
		Tk_CreateTimerHandler( timeout->tv_sec * 1000 + timeout->tv_usec / 1000,
				       TkTimerProc, (ClientData) this );

	while ( ! glish_select && ! s->NotificationQueueLength() )
		Tk_DoOneEvent( 0 );

	return selector_status;
	}

#endif
