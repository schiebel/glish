// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//

#ifndef tkcanvas_h
#define tkcanvas_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"
#include "TkAgent.h"
//
//  For glish_declare(Dict,int)
//
#include "Glish/Client.h"

typedef Dict(int) canvas_item_count;

class TkCanvas : public TkAgent {
    public:
	TkCanvas( Sequencer *, TkFrame *, charptr width, charptr height, const Value *region_,
		  charptr relief, charptr borderwidth, charptr background, charptr fill );

	void UnMap();

	const char **PackInstruction();

	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );

	void ButtonEvent(const char *event, IValue *rec);

	void Add(TkFrame *item) {  frame_list.append(item); }
	void Remove(TkFrame *item) { frame_list.remove(item); }

	static IValue *Create( Sequencer *, const_args_list *);
	~TkCanvas();
	int CanvasCount() const { return count; }
	int ItemCount(const char *) const;
	int NewItemCount(const char *);
	Sequencer *seq() { return sequencer; }
	int CanExpand() const;
    protected:
	char *fill;
	static int count;
	canvas_item_count item_count;
	tkagent_list frame_list;
	};

#endif
#endif
