// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//
#ifndef tkpgplot_h_
#define tkpgplot_h_
#include "Glish/gtk.h"

class TkPgplot : public TkProxy {
    public:
	TkPgplot (ProxyStore *, TkFrame *, charptr width, charptr height, 
		  const Value *region_, const Value *axis_, const Value *nxsub, 
		  const Value *nysub, charptr relief_, charptr borderwidth, 
		  charptr padx, charptr pady, charptr foreground, charptr background,
		  charptr fill_, int mincolor, int maxcolor, int cmap_share, int cmap_fail );
	TkPgplot (ProxyStore *, const Value *idv, const Value *region_, const Value *axis_,
		  const Value *nxsub, const Value *nysub );

	static void Create (ProxyStore *, Value * );

	~TkPgplot ();

	int IsValid() const;
	void UnMap ();
	void yScrolled (const double *firstlast);
	void xScrolled (const double *firstlast);

	const char **PackInstruction ();

	int CanExpand () const;

	// Standard PGPLOT routines.
	char *Pgarro (Value *);
	char *Pgask (Value *);
	char *Pgbbuf (Value *);
	char *Pgbeg (Value *);
	char *Pgbin (Value *);
	char *Pgbox (Value *);
	char *Pgcirc (Value *);
	char *Pgclos (Value *);
	char *Pgconb (Value *);
	char *Pgconl (Value *);
	char *Pgcons (Value *);
	char *Pgcont (Value *);
	char *Pgctab (Value *);
	char *Pgdraw (Value *);
	char *Pgebuf (Value *);
	char *Pgend (Value *);
	char *Pgenv (Value *);
	char *Pgeras (Value *);
	char *Pgerrb (Value *);
	char *Pgerrx (Value *);
	char *Pgerry (Value *);
	char *Pgetxt (Value *);
	char *Pggray (Value *);
	char *Pghi2d (Value *);
	char *Pghist (Value *);
	char *Pgiden (Value *);
	char *Pgimag (Value *);
	char *Pglab (Value *);
	char *Pgldev (Value *);
	char *Pglen (Value *);
	char *Pgline (Value *);	
	char *Pgmove (Value *);
	char *Pgmtxt (Value *);
	char *Pgnumb (Value *);
	char *Pgopen (Value *);
	char *Pgpage (Value *);
	char *Pgpanl (Value *);
	char *Pgpap (Value *);
	char *Pgpixl (Value *);
	char *Pgpnts (Value *);
	char *Pgpoly (Value *);
	char *Pgpt (Value *);
	char *Pgptxt (Value *);
	char *Pgqah (Value *);
	char *Pgqcf (Value *);
	char *Pgqch (Value *);
	char *Pgqci (Value *);
	char *Pgqcir (Value *);
	char *Pgqcol (Value *);
	char *Pgqcr (Value *);
	char *Pgqcs (Value *);
	char *Pgqfs (Value *);
	char *Pgqhs (Value *);
	char *Pgqid (Value *);
	char *Pgqinf (Value *);
	char *Pgqitf (Value *);
	char *Pgqls (Value *);
	char *Pgqlw (Value *);
	char *Pgqpos (Value *);
	char *Pgqtbg (Value *);
	char *Pgqtxt (Value *);
	char *Pgqvp (Value *);
	char *Pgqvsz (Value *);
	char *Pgqwin (Value *);
	char *Pgrect (Value *);
	char *Pgrnd (Value *);
	char *Pgrnge (Value *);
	char *Pgsah (Value *);
	char *Pgsave (Value *);
	char *Pgscf (Value *);
	char *Pgsch (Value *);
	char *Pgsci (Value *);
	char *Pgscir (Value *);
	char *Pgscr (Value *);
	char *Pgscrn (Value *);
	char *Pgsfs (Value *);
	char *Pgshls (Value *);
	char *Pgshs (Value *);
	char *Pgsitf (Value *);
	char *Pgslct (Value *);
	char *Pgsls (Value *);
	char *Pgslw (Value *);
	char *Pgstbg (Value *);
	char *Pgsubp (Value *);
	char *Pgsvp (Value *);
	char *Pgswin (Value *);
	char *Pgtbox (Value *);
	char *Pgtext (Value *);
	char *Pgunsa (Value *);
	char *Pgupdt (Value *);
	char *Pgvect (Value *);
	char *Pgvsiz (Value *);
	char *Pgvstd (Value *);
	char *Pgwedg (Value *);
	char *Pgwnad (Value *);

	// change cursor
	char *Cursor (Value *);

    protected:
	int is_valid;
	int id;
	char *fill;
};

class PgProc : public TkProc {
    public:

	PgProc(TkPgplot *f, char *(TkPgplot::*p)(Value*), TkStrToValProc cvt = 0)
			: TkProc(f,cvt), pgproc(p) { }

	PgProc(const char *c, TkEventProc p, TkStrToValProc cvt = 0)
			: TkProc(c,p,cvt), pgproc(0) { }

	PgProc(TkPgplot *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: TkProc(a,c,p,cvt), pgproc(0) { }

	virtual Value *operator()(Tcl_Interp*, Tk_Window s, Value *arg);

    protected:
	char *(TkPgplot::*pgproc)(Value*);
};

#endif
