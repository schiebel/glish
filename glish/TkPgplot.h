// $Header$

#ifndef tkpgplot_h_
#define tkpgplot_h_

#ifdef GLISHTK
#ifdef TKPGPLOT
#include "Agent.h"
#include "BuiltIn.h"
#include "TkAgent.h"

class TkPgplot : public TkAgent
{
public:
  TkPgplot (Sequencer *, TkFrame *, charptr width, charptr height, 
	    const IValue *region_, const IValue *axis_, const IValue *nxsub, 
	    const IValue *nysub, charptr relief_, charptr borderwidth, 
	    charptr padx, charptr pady, charptr foreground, charptr background,
	    charptr fill_, int mincolor, int maxcolor, int cmap_share, int cmap_fail );

  static IValue *Create (Sequencer *, const_args_list *);

  ~TkPgplot ();

  void UnMap ();
  void yScrolled (const double *firstlast);
  void xScrolled (const double *firstlast);

  const char **PackInstruction ();

  int CanExpand () const;

  // Standard PGPLOT routines.
  char *Pgarro (parameter_list *, int, int);
  char *Pgask (parameter_list *, int, int);
  char *Pgbbuf (parameter_list *, int, int);
  char *Pgbeg (parameter_list *, int, int);
  char *Pgbin (parameter_list *, int, int);
  char *Pgbox (parameter_list *, int, int);
  char *Pgcirc (parameter_list *, int, int);
  char *Pgclos (parameter_list *, int, int);
  char *Pgconb (parameter_list *, int, int);
  char *Pgconl (parameter_list *, int, int);
  char *Pgcons (parameter_list *, int, int);
  char *Pgcont (parameter_list *, int, int);
  char *Pgctab (parameter_list *, int, int);
  char *Pgdraw (parameter_list *, int, int);
  char *Pgebuf (parameter_list *, int, int);
  char *Pgend (parameter_list *, int, int);
  char *Pgenv (parameter_list *, int, int);
  char *Pgeras (parameter_list *, int, int);
  char *Pgerrb (parameter_list *, int, int);
  char *Pgerrx (parameter_list *, int, int);
  char *Pgerry (parameter_list *, int, int);
  char *Pgetxt (parameter_list *, int, int);
  char *Pggray (parameter_list *, int, int);
  char *Pghi2d (parameter_list *, int, int);
  char *Pghist (parameter_list *, int, int);
  char *Pgiden (parameter_list *, int, int);
  char *Pgimag (parameter_list *, int, int);
  char *Pglab (parameter_list *, int, int);
  char *Pgldev (parameter_list *, int, int);
  char *Pglen (parameter_list *, int, int);
  char *Pgline (parameter_list *, int, int);	
  char *Pgmove (parameter_list *, int, int);
  char *Pgmtxt (parameter_list *, int, int);
  char *Pgnumb (parameter_list *, int, int);
  char *Pgopen (parameter_list *, int, int);
  char *Pgpage (parameter_list *, int, int);
  char *Pgpanl (parameter_list *, int, int);
  char *Pgpap (parameter_list *, int, int);
  char *Pgpixl (parameter_list *, int, int);
  char *Pgpnts (parameter_list *, int, int);
  char *Pgpoly (parameter_list *, int, int);
  char *Pgpt (parameter_list *, int, int);
  char *Pgptxt (parameter_list *, int, int);
  char *Pgqah (parameter_list *, int, int);
  char *Pgqcf (parameter_list *, int, int);
  char *Pgqch (parameter_list *, int, int);
  char *Pgqci (parameter_list *, int, int);
  char *Pgqcir (parameter_list *, int, int);
  char *Pgqcol (parameter_list *, int, int);
  char *Pgqcr (parameter_list *, int, int);
  char *Pgqcs (parameter_list *, int, int);
  char *Pgqfs (parameter_list *, int, int);
  char *Pgqhs (parameter_list *, int, int);
  char *Pgqid (parameter_list *, int, int);
  char *Pgqinf (parameter_list *, int, int);
  char *Pgqitf (parameter_list *, int, int);
  char *Pgqls (parameter_list *, int, int);
  char *Pgqlw (parameter_list *, int, int);
  char *Pgqpos (parameter_list *, int, int);
  char *Pgqtbg (parameter_list *, int, int);
  char *Pgqtxt (parameter_list *, int, int);
  char *Pgqvp (parameter_list *, int, int);
  char *Pgqvsz (parameter_list *, int, int);
  char *Pgqwin (parameter_list *, int, int);
  char *Pgrect (parameter_list *, int, int);
  char *Pgrnd (parameter_list *, int, int);
  char *Pgrnge (parameter_list *, int, int);
  char *Pgsah (parameter_list *, int, int);
  char *Pgsave (parameter_list *, int, int);
  char *Pgscf (parameter_list *, int, int);
  char *Pgsch (parameter_list *, int, int);
  char *Pgsci (parameter_list *, int, int);
  char *Pgscir (parameter_list *, int, int);
  char *Pgscr (parameter_list *, int, int);
  char *Pgscrn (parameter_list *, int, int);
  char *Pgsfs (parameter_list *, int, int);
  char *Pgshls (parameter_list *, int, int);
  char *Pgshs (parameter_list *, int, int);
  char *Pgsitf (parameter_list *, int, int);
  char *Pgslct (parameter_list *, int, int);
  char *Pgsls (parameter_list *, int, int);
  char *Pgslw (parameter_list *, int, int);
  char *Pgstbg (parameter_list *, int, int);
  char *Pgsubp (parameter_list *, int, int);
  char *Pgsvp (parameter_list *, int, int);
  char *Pgswin (parameter_list *, int, int);
  char *Pgtbox (parameter_list *, int, int);
  char *Pgtext (parameter_list *, int, int);
  char *Pgunsa (parameter_list *, int, int);
  char *Pgupdt (parameter_list *, int, int);
  char *Pgvect (parameter_list *, int, int);
  char *Pgvsiz (parameter_list *, int, int);
  char *Pgvstd (parameter_list *, int, int);
  char *Pgwedg (parameter_list *, int, int);
  char *Pgwnad (parameter_list *, int, int);

  // change cursor
  char *Cursor (parameter_list *, int, int);
  // KeyPress, KeyRelease
  void CursorEvent (const char *name, const char *type, const char *key, 
		    int *pt);
  // ButtonPress, ButtonRelease
  void CursorEvent (const char *name, const char *type, int button, int *pt);
  // MotionNotify, LeaveNotify, EnterNotify
  void CursorEvent (const char *name, const char *type, int *coord);
  // <other> event
  void CursorEvent (const char *name, const char *type);

protected:
  int id;
  char *fill;
};

#endif
#endif
#endif

// Local Variables:
// mode: c++
// End:
