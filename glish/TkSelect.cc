// $Id$
// Copyright (c) 1997 Associated Universities Inc.
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
	if ( s->type() == Selectee::READ )
		Tk_CreateFileHandler( s->FD(), TK_READABLE, TkFileProc, (ClientData) this );
	else
		Tk_CreateFileHandler( s->FD(), TK_WRITABLE, TkFileProc, (ClientData) this );
	}

void TkSelector::DeleteSelectee( int selectee_fd )
	{
	Tk_DeleteFileHandler( selectee_fd );
	Selector::DeleteSelectee( selectee_fd );
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

	while ( ! glish_select && ! s->NotificationQueueLength() && ! break_selection )
		TkAgent::DoOneTkEvent( );

	if ( break_selection )
		{
		break_selection = 0;
		selector_status = 0;
		}

	return selector_status;
	}

#endif
