#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "Glish/Proxy.h"
#include "tkCore.h"
#include "tkCanvas.h"

extern "C" void GlishTk_init( ProxyStore * );

void GlishTk_init( ProxyStore *store )
	{
	TkProxy::set_global_store( store );

	store->Register( "frame", TkFrameP::Create );
	store->Register( "button", TkButton::Create );
	store->Register( "scale", TkScale::Create );
	store->Register( "text", TkText::Create );
	store->Register( "scrollbar", TkScrollbar::Create );
	store->Register( "label", TkLabel::Create );
	store->Register( "entry", TkEntry::Create );
	store->Register( "message", TkMessage::Create );
	store->Register( "listbox", TkListbox::Create );
	store->Register( "canvas", TkCanvas::Create );
	store->Register( "version", TkProxy::Version );
	store->Register( "have_gui", TkProxy::HaveGui );
	store->Register( "tk_hold", TkProxy::HoldEvents );
	store->Register( "tk_release", TkProxy::ReleaseEvents );
	store->Register( "tk_iconpath", TkProxy::SetBitmapPath );
	store->Register( "tk_checkcolor", TkProxy::CheckColor );

	store->Register( "tk_load", TkProxy::Load );
	store->Register( "tk_loadpath", TkProxy::SetLoadPath );
	}
