// $Header$

#ifndef tkcanvas_h
#define tkcanvas_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"
#include "TkAgent.h"
//
//  For declare(Dict,int)
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

	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkCanvas();
	int CanvasCount() const { return count; }
	int ItemCount(const char *) const;
	int NewItemCount(const char *);
	Sequencer *seq() { return sequencer; }
    protected:
	char *fill;
	static int count;
	canvas_item_count item_count;
	tkagent_list frame_list;
	};

#endif
#endif
