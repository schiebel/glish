// $Header$

#ifndef tkcanvas_h
#define tkcanvas_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"
#include "TkAgent.h"

typedef unsigned long unsigned_long;
declare(Dict,unsigned_long);
typedef Dict(unsigned_long) canvas_item_count;

class TkCanvas : public TkAgent {
    public:
	TkCanvas( Sequencer *, TkFrame *, charptr width, charptr height, const Value *region_,
		  charptr relief, charptr borderwidth, charptr background );

	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );

	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkCanvas();
	unsigned long CanvasCount() const { return count; }
	unsigned long ItemCount(const char *) const;
	unsigned long NewItemCount(const char *);
    protected:
	static unsigned long count;
	canvas_item_count item_count;
	};

#endif
#endif
