// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "TkSelect.h"

#ifdef GLISHTK
#include "Sequencer.h"
#include "TkAgent.h"
#include "Rivet/tk.h"

static int selector_status = 0;
static int glish_select = 0;

void glish_event_posted( int status )
	{
	glish_select = 1;
	selector_status = status;
	}

void TkFileProc ( ClientData data, int )
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

void TkSelector::AddSelectee( Selectee *S )
	{
	Selector::AddSelectee( S );
	if ( S->type() == Selectee::READ )
		Tk_CreateFileHandler( S->FD(), TK_READABLE, TkFileProc, (ClientData) this );
	else
		Tk_CreateFileHandler( S->FD(), TK_WRITABLE, TkFileProc, (ClientData) this );
	}

void TkSelector::DeleteSelectee( int selectee_fd, Selectee *replacement )
	{
	if ( selectee_fd < 0 )
		return;
	Tk_DeleteFileHandler( selectee_fd );
	Selector::DeleteSelectee( selectee_fd, replacement );
	}

int TkSelector::DoSelection( int )
	{
	if ( TkAgent::GlishEventsHeld() )
		TkAgent::FlushGlishEvents();

	if ( await_done )
		{
		await_done = 0;
		return 1;
		}

	glish_select = 0;

	int orig_queue_len = s->NotificationQueueLength();
	while ( ! glish_select &&  s->NotificationQueueLength() == orig_queue_len && ! break_selection )
		TkAgent::DoOneTkEvent( );

	if ( break_selection )
		{
		break_selection = 0;
		selector_status = 0;
		}

	return selector_status;
	}

#endif
