// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#ifdef GLISHTK
#ifdef TKPGPLOT

#include "tkPgplot.h"

#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "system.h"
//
// PGPLOT routines et al.
//
#ifndef USE_RIVET
#define USE_RIVET
#endif
#include "rvpgplot.h"
#include "cpgplot.h"

extern ProxyStore *global_store;
extern Rivetclass PgplotClass;

#define InvalidArg( num )						\
	{								\
	global_store->Error( "invalid type for argument " ## #num );	\
	return;								\
	}

#define InvalidNumberOfArgs( num )					\
	{								\
	global_store->Error( "invalid number of arguments, " ## #num ## " expected" );\
	return;								\
	}

#define SETINIT								\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value");			\
		return;							\
		}							\
									\
	Ref( args );							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

#define SETDONE Unref(args);

#define SETVAL(var,condition)						\
	const Value *var      = rptr->NthEntry( c++, key );		\
	if ( ! ( condition) )						\
		InvalidArg(c-1);

#define SETINT(var)							\
	SETVAL(var##_v_, var##_v_ ->IsNumeric() &&			\
				var##_v_ ->Length() > 0 )		\
	int var = var##_v_ ->IntVal();

#define SETSTR(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0 )		\
	charptr var = ( var##_v_ ->StringPtr(0) )[0];
#define SETDIM(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0   ||		\
				var##_v_ ->IsNumeric() )		\
	char var##_char_[30];						\
	charptr var = 0;						\
	if ( var##_v_ ->Type() == TYPE_STRING )				\
		var = ( var##_v_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		sprintf(var##_char_,"%d", var##_v_ ->IntVal());	\
		var = var##_char_;					\
		}

#define GETSTART(var)							\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad argument value");		\
		return 0;						\
		}							\
									\
	int arg_len = args->Length ();					\
	if (arg_len != var) {						\
		global_store->Error("Wrong number of arguments");	\
		return 0;						\
	}								\
	int c = 0;							\
	int len = 0;							\
	recordptr rptr = args->RecordPtr(0);				\
	const char *key;

#define GETEXPR(var)

#define GETVAL(var)							\
	GETEXPR (var);							\
	const Value *var##_val = rptr->NthEntry( c++, key );		\
	if (!var##_val->IsNumeric () || var##_val->Length () <= 0) {	\
		global_store->Error("bad argument, numeric expected");	\
		return 0;						\
	}								\
	if (var##_val->Length () > len) {				\
		len = var##_val->Length ();				\
	}

#define GETFLOATARRAY(var)						\
	len = 0;							\
	GETVAL (var);							\
	int var##_is_copy = 0;						\
	float *var = var##_val->CoerceToFloatArray (var##_is_copy, len); \
	if (!var##_is_copy) {						\
                float *v = (float *)alloc_memory (sizeof (float) * len); \
		memcpy (v, var, len * sizeof (float));			\
		var = v;						\
	}

#define GETINTARRAY(var)						\
	len = 0;							\
	GETVAL (var);							\
	int var##_is_copy = 0;						\
	int *var = var##_val->CoerceToIntArray (var##_is_copy, len);	\
	if (!var##_is_copy) {						\
                int *v = (int *)alloc_memory (sizeof (int) * len);      \
		memcpy (v, var, len * sizeof (int));			\
		var = v;						\
	}

#define GETBOOLEAN(var)							\
	GETVAL (var);							\
	glish_bool var = var##_val->BoolVal ();

#define GETFLOAT(var)							\
	GETVAL (var);							\
	float var = var##_val->FloatVal ();

#define GETINT(var)							\
	GETVAL (var);							\
	int var = var##_val->IntVal ();

#define GETSTRING(var)							\
	GETEXPR (var);							\
	const Value *var##_val = rptr->NthEntry( c++, key );		\
	if (var##_val->Length () <= 0) {				\
		{							\
		global_store->Error("zero length argument");		\
		return 0;						\
		}							\
	}								\
	char *var = var##_val->StringVal ();

#define GETDONEARRAY(var)						\
	free_memory (var);

#define GETDONESINGLE(var)

#define GETDONESTRING(var)						\
	free_memory (var);

#define EXPRINIT(EVENT)							\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
									\
	/*Ref(args);*/							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

// JAU: Only used once; could be removed.
#define EXPRDIM(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
	charptr var = 0;						\
	char var##_char_[30];						\
	if ( ! var##_val_ || ( var##_val_ ->Type() != TYPE_STRING &&	\
			       ! var##_val_ ->IsNumeric() ) ||		\
		var##_val_ ->Length() <= 0 )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	else								\
		if ( var##_val_ ->Type() == TYPE_STRING	)		\
			var = ( var##_val_ ->StringPtr(0) )[0];		\
		else							\
			{						\
			sprintf(var##_char_,"%d", var##_val_->IntVal());\
			var = var##_char_;				\
			}

#define EXPRSTRVALXX(var,EVENT,LINE)					\
	const Value *var = rptr->NthEntry( c++, key );			\
	LINE								\
	if ( ! var || var ->Type() != TYPE_STRING ||			\
		var->Length() <= 0 )					\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}

// JAU: Only used once; could be removed.
#define EXPRSTRVAL(var,EVENT) EXPRSTRVALXX(var,EVENT,const Value *var##_val_ = var;)

#define EXPRSTR(var,EVENT)						\
	charptr var = 0;						\
	EXPRSTRVALXX(var##_val_, EVENT,)				\
	var = ( var##_val_ ->StringPtr(0) )[0];

#define EXPR_DONE(var)

// JAU: Check for memory leakage.
#define GETSHAPE(var)							\
	const attributeptr attr = var##_val->AttributePtr ();		\
	const Value *shape_val;						\
	int shape_len;							\
	if (!attr || !(shape_val = (*attr)["shape"]) ||			\
	    !shape_val->IsNumeric () ||					\
	    (shape_len = shape_val->Length ()) != 2) {			\
		return 0;						\
	}								\
	int idim = shape_val->IntVal (1);				\
	int jdim = shape_val->IntVal (2);

#define CREATE_RETURN						\
	if ( ! ret || ! ret->IsValid() )			\
		{						\
		Value *err = ret->GetError();			\
		if ( err )					\
			{					\
			global_store->Error( err );		\
			Unref( err );				\
			}					\
		else						\
			global_store->Error( "tk widget creation failed" ); \
		}						\
	else							\
		ret->SendCtor("newtk");				\
								\
	SETDONE

static int colorcells_available(Rivetobj obj, int needed)
{
  TkWindow *w = obj->tkwin;
  Colormap  cmap = Tk_Colormap(w);

  unsigned long planes[1];
  unsigned int nplanes = 0;
  unsigned long *pixel = (unsigned long *) alloc_memory(sizeof(unsigned long) * needed);

//
// See if we can get all of the colors requested.
//
  int ret = 0;
  if(XAllocColorCells(Tk_Display(w), cmap, False, planes, nplanes,
		      pixel, (unsigned) needed)) {
    XFreeColors(Tk_Display(w), cmap, pixel, (int) needed, (unsigned long)0);
    ret = 1;
  }

  free_memory(pixel);
  return ret;
}

void
TkPgplot::UnMap ()
{
  if (self) {
    cpgslct (id);
    cpgclos ();
    TkAgent::UnMap ();
  }
}

TkPgplot::~TkPgplot ()
{
  if (frame)
    {
      frame->RemoveElement (this);
      frame->Pack ();
    }
  UnMap ();
}

int
pgplot_yscrollcb (Rivetobj button, XEvent *unused1, ClientData assoc,
		  ClientData calldata)
{
  double *firstlast = (double *)calldata;

  ((TkPgplot *)assoc)->yScrolled (firstlast);

  return TCL_OK;
}

int
pgplot_xscrollcb (Rivetobj button, XEvent *unused1, ClientData assoc,
		  ClientData calldata)
{
  double *firstlast = (double *)calldata;

  ((TkPgplot *)assoc)->xScrolled (firstlast);

  return TCL_OK;
}

struct tk_farrayRec
{
  int len;
  float *val;
};

struct tk_iarrayRec
{
  int len;
  int *val;
};

extern Value *glishtk_str (char *str);

Value *
tk_castfToStr (char *str)
{
  tk_farrayRec *floats = (tk_farrayRec *)str;

  return new Value (floats->val, floats->len);
}

Value *
tk_castiToStr (char *str)
{
  tk_iarrayRec *ints = (tk_iarrayRec *)str;

  return new Value (ints->val, ints->len);
}

struct glishtk_pgplot_bindinfo
{
  TkPgplot *pgplot;
  char *event_name;
  char *tk_event_name;
  glishtk_pgplot_bindinfo (TkPgplot *c, const char *event,
			   const char *tk_event) :
    tk_event_name (strdup (tk_event)), event_name (strdup (event)), pgplot (c)
    {
    }
  ~glishtk_pgplot_bindinfo ()
    {
      free_memory (tk_event_name);
      free_memory (event_name);
    }
};

int
glishtk_pgplot_entercb (Rivetobj pgplot, XEvent *xevent, ClientData assoc,
			int ks, int callbacktype)
{
  rivet_set_focus (pgplot);

  return TCL_OK;
}

int
glishtk_pgplot_buttoncb (Rivetobj pgplot, XEvent *xevent, ClientData assoc,
			 int ks, int callbacktype)
{
  static char *event_names[] =
  {
    "",
    "",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify"
  };
  glishtk_pgplot_bindinfo *info = (glishtk_pgplot_bindinfo *)assoc;
  int device[2];
  char key[32];
  KeySym keysym;		// Key code of pressed keyboard key.
  int nret;			// Number of characters returned in buffer[].

  switch (xevent->type)
    {
    case KeyPress:
    case KeyRelease:
      device[0] = xevent->xkey.x;
      device[1] = xevent->xkey.y;
      nret = XLookupString (&xevent->xkey, key,
			    (int)(sizeof (key) / sizeof (char)), &keysym, NULL);
      if (nret) {
	key[nret] = '\0';
      }
      info->pgplot->CursorEvent (info->event_name, event_names[xevent->type],
				 nret > 0 ? key : "<UNKNOWN>", device);
      break;

    case ButtonPress:
    case ButtonRelease:
      device[0] = xevent->xbutton.x;
      device[1] = xevent->xbutton.y;
      info->pgplot->CursorEvent (info->event_name, event_names[xevent->type],
				 xevent->xbutton.button, device);
      break;

    case MotionNotify:
      device[0] = xevent->xmotion.x;
      device[1] = xevent->xmotion.y;
      info->pgplot->CursorEvent (info->event_name, event_names[xevent->type],
				 device);
      break;

    case LeaveNotify:
    case EnterNotify:
      device[0] = xevent->xcrossing.x;
      device[1] = xevent->xcrossing.y;
      info->pgplot->CursorEvent (info->event_name, event_names[xevent->type],
				 device);
      break;

    default:
      info->pgplot->CursorEvent (info->event_name, event_names[xevent->type]);
    }
  return TCL_OK;
}

char *
glishtk_pgplot_bind (TkAgent *agent, const char *cmd, Value *args)
{
  char *event_name = "pgplot bind function";
  int c = 0;

  if (args->Length () >= 2) {
    EXPRINIT(event_name)
    EXPRSTR (button, event_name);
    EXPRSTR (event, event_name);
    glishtk_pgplot_bindinfo *binfo =
      new glishtk_pgplot_bindinfo ((TkPgplot *)agent, event, button);

    if (rivet_create_binding (agent->Self (), 0, (char *)button,
			      (int (*)())glishtk_pgplot_buttoncb,
			      (ClientData)binfo, 1, 0) == TCL_ERROR) {
      global_store->Error( "binding not created." );
      delete binfo;
    }
    EXPR_DONE (event);
    EXPR_DONE (button);
  }
  return 0;
}

Value *
glishtk_int (char *sel)
{
  return new Value (atoi (sel));
}

char *
glishtk_oneornodim (Rivetobj self, const char *cmd, Value *args)
{
  char *event_name = "one or zero dim function";

  if (args->Length () > 0) {

    EXPRINIT(event_name)
    EXPRDIM (dim, event_name);
    rivet_set (self, (char *)cmd, (char *)dim);
    EXPR_DONE (dim);

    return 0;
  } else {
    return rivet_va_cmd (self, "cget", (char *)cmd, 0);
  }
}

// JAU: Will need to change glish.init et al. for rivet-less behavior.
TkPgplot::TkPgplot (ProxyStore *s, TkFrame *frame_, charptr width,
		    charptr height, const Value *region_, const Value *axis_,
		    const Value *nxsub_, const Value *nysub_, charptr relief_,
		    charptr borderwidth, charptr padx, charptr pady, charptr foreground,
		    charptr background, charptr fill_, int mincolors, int maxcolors, int cmap_share, int cmap_fail ) :
		TkAgent (s), fill (0)
{
  frame = frame_;
  Rivetobj frameSelf = frame->Self ();
  int region_is_copy = 0;
  int axis_is_copy = 0;
  float *region = 0;
  int axis = 0;
  int nxsub = 1, nysub = 1;
  char maxcolors_str[20];
  char mincolors_str[20];

  agent_ID = "<graphic:pgplot>";

  // JAU: Make rivet-less.
  if (!frame || !frameSelf) {
    return;
  }
  // JAU: If in 8-bit mode, check color availability.  If > 8-bit,
  // assume we've got enough.  A bit kludgy; will be refined later.
  if (cmap_fail && DisplayPlanes (frameSelf->tkwin->display,
				  frameSelf->tkwin->screenNum) <= 8 &&
      !colorcells_available (frameSelf, mincolors)) {
    SetError ((Value *)generate_error ("Not enough color cells available"));
    frame = 0;
    return;
  }

  int c = 2;
  char *argv[32];

  sprintf(maxcolors_str,"%d",maxcolors);
  sprintf(mincolors_str,"%d",mincolors);

  argv[0] = argv[1] = 0;
  argv[c++] = "-width";
  argv[c++] = (char *)width;
  argv[c++] = "-height";
  argv[c++] = (char *)height;
  argv[c++] = "-relief";
  argv[c++] = (char *)relief_;
  argv[c++] = "-borderwidth";
  argv[c++] = (char *)borderwidth;
  argv[c++] = "-padx";
  argv[c++] = (char *)padx;
  argv[c++] = "-pady";
  argv[c++] = (char *)pady;
  argv[c++] = "-foreground";
  argv[c++] = (char *)foreground;
  argv[c++] = "-background";
  argv[c++] = (char *)background;
  argv[c++] = "-mincolors";
  argv[c++] = (char *)mincolors_str;
  argv[c++] = "-maxcolors";
  argv[c++] = (char *)maxcolors_str;
  argv[c++] = "-share";
  argv[c++] = cmap_share ? "true" : "false";
  argv[c++] = "-xscrollcommand";
  argv[c++] = rivet_new_callback ((int (*)())pgplot_xscrollcb,
				  (ClientData)this, 0);
  argv[c++] = "-yscrollcommand";
  argv[c++] = rivet_new_callback ((int (*)())pgplot_yscrollcb,
				  (ClientData)this, 0);

  // JAU: Make rivet-less.
  self = rivet_create (PgplotClass, frameSelf, c, argv);

  if (!self) {
    SetError ((Value *)generate_error ("Rivet creation failed in TkPgplot::TkPgplot"));
    return;
  }
  // JAU: Make rivet-less.
  id = rvp_device_id (self);

  // JAU: Make rivet-less.
  if ( id <= 0 ) {
    id = cpgopen (rvp_device_name (self));
  } else {
    cpgslct (id);
  }
  if (nxsub_->IsNumeric () && nxsub_->Length () > 0) {
    nxsub = nxsub_->IntVal ();
  }
  if (nysub_->IsNumeric () && nysub_->Length () > 0) {
    nysub = nysub_->IntVal ();
  }
  cpgsubp (nxsub, nysub);
  cpgask (0);
  cpgpage ();

  if (region_->Length () >= 4) {
    region = region_->CoerceToFloatArray (region_is_copy, 4);
  }
  if (axis_->IsNumeric () && axis_->Length () > 0) {
    axis = axis_->IntVal ();
  }
  if (region) {
    cpgenv (region[0], region[1], region[2], region[3], 0, axis);
  }
  if (region_is_copy) {
    free_memory (region);
  }
  if (fill_ && fill_[0] && strcmp (fill_, "none")) {
    fill = strdup (fill_);
  }
  // JAU: Make rivet-less.
  frame->AddElement (this);
  frame->Pack ();
  rivet_create_binding (self, 0, "<Enter>", (int (*)())glishtk_pgplot_entercb,
			0, 1, 0);
  // Non-standard routines.
  procs.Insert ("bind", new TkProc (this, "", glishtk_pgplot_bind));
  procs.Insert ("cursor", new TkProc (this, &TkPgplot::Cursor, glishtk_str));
  procs.Insert ("height", new TkProc ("-height", glishtk_oneornodim,
				      glishtk_int));
  procs.Insert ("view", new TkProc ("", glishtk_scrolled_update));
  procs.Insert ("width", new TkProc ("-width", glishtk_oneornodim,
				     glishtk_int));

  procs.Insert ("padx", new TkProc ("-padx", glishtk_oneornodim, glishtk_int));
  procs.Insert ("pady", new TkProc ("-pady", glishtk_oneornodim, glishtk_int));

  // Standard PGPLOT routines.
  procs.Insert ("arro", new TkProc (this, &TkPgplot::Pgarro));
  procs.Insert ("ask", new TkProc (this, &TkPgplot::Pgask));
  procs.Insert ("bbuf", new TkProc (this, &TkPgplot::Pgbbuf));
  procs.Insert ("beg", new TkProc (this, &TkPgplot::Pgbeg));
  procs.Insert ("bin", new TkProc (this, &TkPgplot::Pgbin));
  procs.Insert ("box", new TkProc (this, &TkPgplot::Pgbox));
  procs.Insert ("circ", new TkProc (this, &TkPgplot::Pgcirc));
  procs.Insert ("clos", new TkProc (this, &TkPgplot::Pgclos));
  procs.Insert ("conb", new TkProc (this, &TkPgplot::Pgconb));
  procs.Insert ("conl", new TkProc (this, &TkPgplot::Pgconl));
  procs.Insert ("cons", new TkProc (this, &TkPgplot::Pgcons));
  procs.Insert ("cont", new TkProc (this, &TkPgplot::Pgcont));
  procs.Insert ("ctab", new TkProc (this, &TkPgplot::Pgctab));
  procs.Insert ("draw", new TkProc (this, &TkPgplot::Pgdraw));
  procs.Insert ("ebuf", new TkProc (this, &TkPgplot::Pgebuf));
  procs.Insert ("end", new TkProc (this, &TkPgplot::Pgend));
  procs.Insert ("env", new TkProc (this, &TkPgplot::Pgenv));
  procs.Insert ("eras", new TkProc (this, &TkPgplot::Pgeras));
  procs.Insert ("errb", new TkProc (this, &TkPgplot::Pgerrb));
  procs.Insert ("errx", new TkProc (this, &TkPgplot::Pgerrx));
  procs.Insert ("erry", new TkProc (this, &TkPgplot::Pgerry));
  procs.Insert ("etxt", new TkProc (this, &TkPgplot::Pgetxt));
  procs.Insert ("gray", new TkProc (this, &TkPgplot::Pggray));
  procs.Insert ("hi2d", new TkProc (this, &TkPgplot::Pghi2d));
  procs.Insert ("hist", new TkProc (this, &TkPgplot::Pghist));
  procs.Insert ("iden", new TkProc (this, &TkPgplot::Pgiden));
  procs.Insert ("imag", new TkProc (this, &TkPgplot::Pgimag));
  procs.Insert ("lab", new TkProc (this, &TkPgplot::Pglab));
  procs.Insert ("ldev", new TkProc (this, &TkPgplot::Pgldev));
  procs.Insert ("len", new TkProc (this, &TkPgplot::Pglen, tk_castfToStr));
  procs.Insert ("line", new TkProc (this, &TkPgplot::Pgline));
  procs.Insert ("move", new TkProc (this, &TkPgplot::Pgmove));
  procs.Insert ("mtxt", new TkProc (this, &TkPgplot::Pgmtxt));
  procs.Insert ("numb", new TkProc (this, &TkPgplot::Pgnumb, glishtk_str));
  procs.Insert ("open", new TkProc (this, &TkPgplot::Pgopen, tk_castiToStr));
  procs.Insert ("page", new TkProc (this, &TkPgplot::Pgpage));
  procs.Insert ("panl", new TkProc (this, &TkPgplot::Pgpanl));
  procs.Insert ("pap", new TkProc (this, &TkPgplot::Pgpap));
  procs.Insert ("pixl", new TkProc (this, &TkPgplot::Pgpixl));
  procs.Insert ("pnts", new TkProc (this, &TkPgplot::Pgpnts));
  procs.Insert ("poly", new TkProc (this, &TkPgplot::Pgpoly));
  procs.Insert ("pt", new TkProc (this, &TkPgplot::Pgpt));
  procs.Insert ("ptxt", new TkProc (this, &TkPgplot::Pgptxt));
  procs.Insert ("qah", new TkProc (this, &TkPgplot::Pgqah, tk_castfToStr));
  procs.Insert ("qcf", new TkProc (this, &TkPgplot::Pgqcf, tk_castiToStr));
  procs.Insert ("qch", new TkProc (this, &TkPgplot::Pgqch, tk_castfToStr));
  procs.Insert ("qci", new TkProc (this, &TkPgplot::Pgqci, tk_castiToStr));
  procs.Insert ("qcir", new TkProc (this, &TkPgplot::Pgqcir, tk_castiToStr));
  procs.Insert ("qcol", new TkProc (this, &TkPgplot::Pgqcol, tk_castiToStr));
  procs.Insert ("qcr", new TkProc (this, &TkPgplot::Pgqcr, tk_castfToStr));
  procs.Insert ("qcs", new TkProc (this, &TkPgplot::Pgqcs, tk_castfToStr));
  procs.Insert ("qfs", new TkProc (this, &TkPgplot::Pgqfs, tk_castiToStr));
  procs.Insert ("qhs", new TkProc (this, &TkPgplot::Pgqhs, tk_castfToStr));
  procs.Insert ("qid", new TkProc (this, &TkPgplot::Pgqid, tk_castiToStr));
  procs.Insert ("qinf", new TkProc (this, &TkPgplot::Pgqinf, glishtk_str));
  procs.Insert ("qitf", new TkProc (this, &TkPgplot::Pgqitf, tk_castiToStr));
  procs.Insert ("qls", new TkProc (this, &TkPgplot::Pgqls, tk_castiToStr));
  procs.Insert ("qlw", new TkProc (this, &TkPgplot::Pgqlw, tk_castiToStr));
  procs.Insert ("qpos", new TkProc (this, &TkPgplot::Pgqpos, tk_castfToStr));
  procs.Insert ("qtbg", new TkProc (this, &TkPgplot::Pgqtbg, tk_castiToStr));
  procs.Insert ("qtxt", new TkProc (this, &TkPgplot::Pgqtxt, tk_castfToStr));
  procs.Insert ("qvp", new TkProc (this, &TkPgplot::Pgqvp, tk_castfToStr));
  procs.Insert ("qvsz", new TkProc (this, &TkPgplot::Pgqvsz, tk_castfToStr));
  procs.Insert ("qwin", new TkProc (this, &TkPgplot::Pgqwin, tk_castfToStr));
  procs.Insert ("rect", new TkProc (this, &TkPgplot::Pgrect));
  procs.Insert ("rnd", new TkProc (this, &TkPgplot::Pgrnd, tk_castfToStr));
  procs.Insert ("rnge", new TkProc (this, &TkPgplot::Pgrnge, tk_castfToStr));
  procs.Insert ("sah", new TkProc (this, &TkPgplot::Pgsah));
  procs.Insert ("save", new TkProc (this, &TkPgplot::Pgsave));
  procs.Insert ("scf", new TkProc (this, &TkPgplot::Pgscf));
  procs.Insert ("sch", new TkProc (this, &TkPgplot::Pgsch));
  procs.Insert ("sci", new TkProc (this, &TkPgplot::Pgsci));
  procs.Insert ("scir", new TkProc (this, &TkPgplot::Pgscir));
  procs.Insert ("scr", new TkProc (this, &TkPgplot::Pgscr));
  procs.Insert ("scrn", new TkProc (this, &TkPgplot::Pgscrn, tk_castiToStr));
  procs.Insert ("sfs", new TkProc (this, &TkPgplot::Pgsfs));
  procs.Insert ("shls", new TkProc (this, &TkPgplot::Pgshls));
  procs.Insert ("shs", new TkProc (this, &TkPgplot::Pgshs));
  procs.Insert ("sitf", new TkProc (this, &TkPgplot::Pgsitf));
  procs.Insert ("slct", new TkProc (this, &TkPgplot::Pgslct));
  procs.Insert ("sls", new TkProc (this, &TkPgplot::Pgsls));
  procs.Insert ("slw", new TkProc (this, &TkPgplot::Pgslw));
  procs.Insert ("stbg", new TkProc (this, &TkPgplot::Pgstbg));
  procs.Insert ("subp", new TkProc (this, &TkPgplot::Pgsubp));
  procs.Insert ("svp", new TkProc (this, &TkPgplot::Pgsvp));
  procs.Insert ("swin", new TkProc (this, &TkPgplot::Pgswin));
  procs.Insert ("tbox", new TkProc (this, &TkPgplot::Pgtbox));
  procs.Insert ("text", new TkProc (this, &TkPgplot::Pgtext));
  procs.Insert ("unsa", new TkProc (this, &TkPgplot::Pgunsa));
  procs.Insert ("updt", new TkProc (this, &TkPgplot::Pgupdt));
  procs.Insert ("vect", new TkProc (this, &TkPgplot::Pgvect));
  procs.Insert ("vsiz", new TkProc (this, &TkPgplot::Pgvsiz));
  procs.Insert ("vstd", new TkProc (this, &TkPgplot::Pgvstd));
  procs.Insert ("wedg", new TkProc (this, &TkPgplot::Pgwedg));
  procs.Insert ("wnad", new TkProc (this, &TkPgplot::Pgwnad));
}

void
TkPgplot::yScrolled (const double *d)
{
  PostTkEvent("yscroll", new Value ((double *)d, 2, COPY_ARRAY));
}

void
TkPgplot::xScrolled (const double *d)
{
  PostTkEvent("xscroll", new Value ((double *)d, 2, COPY_ARRAY));
}

//
//--- --- --- --- --- --- cursor interaction --- --- --- --- --- ---
//
#define CURSOR_name_match(NAME, NUM)					\
	if (!strcmp (name, #NAME))					\
		{							\
		if ( item[NUM] ) free_memory( item[NUM] );		\
		item[NUM] = val->StringVal();				\
		}

#define CURSOR_match							\
{									\
	CURSOR_name_match (mode, 0)					\
	else CURSOR_name_match (x, 1)					\
	else CURSOR_name_match (y, 2)					\
	else CURSOR_name_match (color, 3)				\
}

char *TkPgplot::Cursor (Value *args)
	{
	static const char *item[4];
	static int init = 0;

	if (!init)
		{
		item[0] = strdup ("norm");
		item[1] = strdup ("0");
		item[2] = strdup ("0");
		item[3] = strdup ("1");
		init = 1;
		}

	if ( args->Type() == TYPE_RECORD )
		{
		recordptr rptr = args->RecordPtr(0);
		for (int c = 0; c < 4 && c < rptr->Length(); ++c)
			{
			const char *name;
			const Value *val = rptr->NthEntry( c, name );
			if ( strncmp( name, "arg", 3 ) )
				CURSOR_match
			else
				{
				if ( item[c] ) free_memory( item[c] );
				item[c] = val->StringVal();
				}
			}
		}
	else
		item[0] = args->StringVal();

	rivet_va_cmd (self, "setcursor", item[0], item[1], item[2], item[3], 0);
	return (char *)item[0];
	}

//PGARRO -- draw an arrow
char *
TkPgplot::Pgarro (Value *args)
{
  GETSTART (4);
  GETFLOAT (x1);
  GETFLOAT (y1);
  GETFLOAT (x2);
  GETFLOAT (y2);
  cpgslct (id);
  cpgarro (x1, y1, x2, y2);
  GETDONESINGLE (x1);
  GETDONESINGLE (y1);
  GETDONESINGLE (x2);
  GETDONESINGLE (y2);

  return "";
}

//PGASK -- control new page prompting
char *
TkPgplot::Pgask (Value *args)
{
  GETSTART (1);
  GETBOOLEAN (ask);
  cpgslct (id);
  cpgask (ask);
  GETDONESINGLE (ask);

  return "";
}

//PGBBUF -- begin batch of output (buffer)
char *
TkPgplot::Pgbbuf (Value *args)
{
  cpgslct (id);
  cpgbbuf ();

  return "";
}

//PGBEG -- begin PGPLOT, open output device
char *
TkPgplot::Pgbeg (Value *args)
{
  GETSTART (4);
  GETINT (unit);
  GETSTRING (device);
  GETINT (nxsub);
  GETINT (nysub);
  cpgslct (id);
  cpgbeg (unit, device, nxsub, nysub);
  GETDONESINGLE (unit);
  GETDONESTRING (device);
  GETDONESINGLE (nxsub);
  GETDONESINGLE (nysub);

  return "";
}

//PGBIN -- histogram of binned data
char *
TkPgplot::Pgbin (Value *args)
{
  GETSTART (3);
  GETFLOATARRAY (x);
  GETFLOATARRAY (data);
  GETBOOLEAN (center);
  cpgslct (id);
  cpgbin (len, x, data, center);
  GETDONEARRAY (x);
  GETDONEARRAY (data);
  GETDONESINGLE (center);

  return "";
}

//PGBOX -- draw labeled frame around viewport
char *
TkPgplot::Pgbox (Value *args)
{
  GETSTART (6);
  GETSTRING (xopt);
  GETFLOAT (xtick);
  GETINT (nxsub);
  GETSTRING (yopt);
  GETFLOAT (ytick);
  GETINT (nysub);
  cpgslct (id);
  cpgbox (xopt, xtick, nxsub, yopt, ytick, nysub);
  GETDONESTRING (xopt);
  GETDONESINGLE (xtick);
  GETDONESINGLE (nxsub);
  GETDONESTRING (yopt);
  GETDONESINGLE (ytick);
  GETDONESINGLE (nysub);

  return "";
}

//PGCIRC -- draw a filled or outline circle
char *
TkPgplot::Pgcirc (Value *args)
{
  GETSTART (3);
  GETFLOAT (xcent);
  GETFLOAT (ycent);
  GETFLOAT (radius);
  cpgslct (id);
  cpgcirc (xcent, ycent, radius);
  GETDONESINGLE (xcent);
  GETDONESINGLE (ycent);
  GETDONESINGLE (radius);

  return "";
}

//PGCLOS -- close the selected graphics device
char *
TkPgplot::Pgclos (Value *args)
{
  cpgslct (id);
  cpgclos ();

  return "";
}

//PGCONB -- contour map of a 2D data array, with blanking
char *
TkPgplot::Pgconb (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOATARRAY (cont);

  int nc = len;			// Length of cont array.

  GETFLOATARRAY (tr);
  GETFLOAT (blank);
  cpgslct (id);
  cpgconb (a, idim, jdim, 1, idim, 1, jdim, cont, nc, tr, blank);
  GETDONEARRAY (a);
  GETDONEARRAY (cont);
  GETDONEARRAY (tr);
  GETDONESINGLE (blank);

  return "";
}

//PGCONL -- label contour map of a 2D data array
char *
TkPgplot::Pgconl (Value *args)
{
  GETSTART (6);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOAT (cont);
  GETFLOATARRAY (tr);
  GETSTRING (label);
  GETINT (intval);
  GETINT (minint);
  cpgslct (id);
  cpgconl (a, idim, jdim, 1, idim, 1, jdim, cont, tr, label, intval, minint);
  GETDONEARRAY (a);
  GETDONESINGLE (cont);
  GETDONEARRAY (tr);
  GETDONESTRING (label);
  GETDONESINGLE (intval);
  GETDONESINGLE (minint);

  return "";
}

//PGCONS -- contour map of a 2D data array (fast algorithm)
char *
TkPgplot::Pgcons (Value *args)
{
  GETSTART (3);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOATARRAY (cont);

  int nc = len;			// Length of cont array.

  GETFLOATARRAY (tr);
  cpgslct (id);
  cpgcons (a, idim, jdim, 1, idim, 1, jdim, cont, nc, tr);
  GETDONEARRAY (a);
  GETDONEARRAY (cont);
  GETDONEARRAY (tr);

  return "";
}

//PGCONT -- contour map of a 2D data array (contour-following)
char *
TkPgplot::Pgcont (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOATARRAY (cont);

  int ncont = len;

  GETBOOLEAN (nc);
  ncont *= nc == glish_true ? 1 : -1;
  GETFLOATARRAY (tr);
  cpgslct (id);
  cpgcont (a, idim, jdim, 1, idim, 1, jdim, cont, ncont, tr);
  GETDONEARRAY (a);
  GETDONEARRAY (cont);
  GETDONESINGLE (nc);
  GETDONEARRAY (tr);

  return "";
}

//PGCTAB -- install the color table to be used by PGIMAG
char *
TkPgplot::Pgctab (Value *args)
{
  GETSTART (6);
  GETFLOATARRAY (l);

  int nc = len;			// Length of l array.

  GETFLOATARRAY (r);
  nc = len < nc ? len : nc;
  GETFLOATARRAY (g);
  nc = len < nc ? len : nc;
  GETFLOATARRAY (b);
  nc = len < nc ? len : nc;
  GETFLOAT (contra);
  GETFLOAT (bright);
  cpgslct (id);
  cpgctab (l, r, g, b, nc, contra, bright);
  GETDONEARRAY (l);
  GETDONEARRAY (r);
  GETDONEARRAY (g);
  GETDONEARRAY (b);
  GETDONESINGLE (contra);
  GETDONESINGLE (bright);

  return "";
}

//PGDRAW -- draw a line from the current pen position to a point
char *
TkPgplot::Pgdraw (Value *args)
{
  GETSTART (2);
  GETFLOAT (x);
  GETFLOAT (y);
  cpgslct (id);
  cpgdraw (x, y);
  GETDONESINGLE (x);
  GETDONESINGLE (y);

  return "";
}

//PGEBUF -- end batch of output (buffer)
char *
TkPgplot::Pgebuf (Value *args)
{
  cpgslct (id);
  cpgebuf ();

  return "";
}

//PGEND -- terminate PGPLOT
char *
TkPgplot::Pgend (Value *args)
{
  // JAU: Using this and then calling ~TkPgplot causes PGPLOT to whine!
  cpgslct (id);
  cpgend ();

  return "";
}

//PGENV -- set window and viewport and draw labeled frame
char *
TkPgplot::Pgenv (Value *args)
{
  static Value xargs;

  GETSTART (6);
  GETFLOAT (xmin);
  GETFLOAT (xmax);
  GETFLOAT (ymin);
  GETFLOAT (ymax);
  GETINT (just);
  GETINT (axis);
  cpgslct (id);
  cpgenv (xmin, xmax, ymin, ymax, just, axis);
  Cursor (&xargs);
  GETDONESINGLE (xmin);
  GETDONESINGLE (xmax);
  GETDONESINGLE (ymin);
  GETDONESINGLE (ymax);
  GETDONESINGLE (just);
  GETDONESINGLE (axis);

  return "";
}

//PGERAS -- erase all graphics from current page
char *
TkPgplot::Pgeras (Value *args)
{
  cpgslct (id);
  cpgeras ();

  return "";
}

//PGERRB -- horizontal or vertical error bar
char *
TkPgplot::Pgerrb (Value *args)
{
  GETSTART (5);
  GETINT (dir);
  GETFLOATARRAY (x);
  GETFLOATARRAY (y);
  GETFLOATARRAY (e);
  GETFLOAT (t);
  cpgslct (id);
  cpgerrb (dir, len, x, y, e, t);
  GETDONESINGLE (dir);
  GETDONEARRAY (x);
  GETDONEARRAY (y);
  GETDONEARRAY (e);
  GETDONESINGLE (t);

  return "";
}

//PGERRX -- horizontal error bar
char *
TkPgplot::Pgerrx (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (x1);
  GETFLOATARRAY (x2);
  GETFLOATARRAY (y);
  GETFLOAT (t);
  cpgslct (id);
  cpgerrx (len, x1, x2, y, t);
  GETDONEARRAY (x1);
  GETDONEARRAY (x2);
  GETDONEARRAY (y);
  GETDONESINGLE (t);

  return "";
}

//PGERRY -- vertical error bar
char *
TkPgplot::Pgerry (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (x);
  GETFLOATARRAY (y1);
  GETFLOATARRAY (y2);
  GETFLOAT (t);
  cpgslct (id);
  cpgerry (len, x, y1, y2, t);
  GETDONEARRAY (x);
  GETDONEARRAY (y1);
  GETDONEARRAY (y2);
  GETDONESINGLE (t);

  return "";
}

//PGETXT -- erase text from graphics display
// JAU: Not listed in current Glish/PGPLOT documentation.
// (An effective no-op anyway....)
char *
TkPgplot::Pgetxt (Value *args)
{
  cpgslct (id);
  cpgetxt ();

  return "";
}

//PGGRAY -- gray-scale map of a 2D data array
char *
TkPgplot::Pggray (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOAT (fg);
  GETFLOAT (bg);
  GETFLOATARRAY (tr);
  cpgslct (id);
  cpggray (a, idim, jdim, 1, idim, 1, jdim, fg, bg, tr);
  GETDONEARRAY (a);
  GETDONESINGLE (fg);
  GETDONESINGLE (bg);
  GETDONEARRAY (tr);

  return "";
}

//PGHI2D -- cross-sections through a 2D data array
char *
TkPgplot::Pghi2d (Value *args)
{
  GETSTART (6);
  GETFLOATARRAY (data);
  GETSHAPE (data);
  GETFLOATARRAY (x);
  GETINT (ioff);
  GETFLOAT (bias);
  GETBOOLEAN (center);
  GETFLOATARRAY (ylims);
  cpgslct (id);
  cpghi2d (data, idim, jdim, 1, idim, 1, jdim, x, ioff, bias, center, ylims);
  GETDONEARRAY (data);
  GETDONEARRAY (x);
  GETDONESINGLE (ioff);
  GETDONESINGLE (bias);
  GETDONESINGLE (center);
  GETDONEARRAY (ylims);

  return "";
}

//PGHIST -- histogram of unbinned data
char *
TkPgplot::Pghist (Value *args)
{
  GETSTART (5);
  GETFLOATARRAY (data);
  GETFLOAT (datmin);
  GETFLOAT (datmax);
  GETINT (nbin);
  GETINT (pgflag);
  cpgslct (id);
  cpghist (len, data, datmin, datmax, nbin, pgflag);
  GETDONEARRAY (data);
  GETDONESINGLE (datmin);
  GETDONESINGLE (datmax);
  GETDONESINGLE (nbin);
  GETDONESINGLE (pgflag);

  return "";
}

//PGIDEN -- write username, date, and time at bottom of plot
char *
TkPgplot::Pgiden (Value *args)
{
  cpgslct (id);
  cpgiden ();

  return "";
}

//PGIMAG -- color image from a 2D data array
char *
TkPgplot::Pgimag (Value *args)
{
  GETSTART (4);
  GETFLOATARRAY (a);
  GETSHAPE (a);
  GETFLOAT (a1);
  GETFLOAT (a2);
  GETFLOATARRAY (tr);
  cpgslct (id);
  cpgimag (a, idim, jdim, 1, idim, 1, jdim, a1, a2, tr);
  GETDONEARRAY (a);
  GETDONESINGLE (a1);
  GETDONESINGLE (a2);
  GETDONEARRAY (tr);

  return "";
}

//PGLAB -- write labels for x-axis, y-axis, and top of plot
char *
TkPgplot::Pglab (Value *args)
{
  GETSTART (3);
  GETSTRING (xlbl);
  GETSTRING (ylbl);
  GETSTRING (toplbl);
  cpgslct (id);
  cpglab (xlbl, ylbl, toplbl);
  GETDONESTRING (xlbl);
  GETDONESTRING (ylbl);
  GETDONESTRING (toplbl);

  return "";
}

//PGLDEV -- list available device types
char *
TkPgplot::Pgldev (Value *args)
{
  cpgslct (id);
  cpgldev ();

  return "";
}

//PGLEN -- find length of a string in a variety of units
char *
TkPgplot::Pglen (Value *args)
{
  GETSTART (2);
  GETINT (units);
  GETSTRING (string);

  static tk_farrayRec qlen;

  qlen.val = (float *)alloc_memory (sizeof (float) * 2);
  qlen.len = 2;
  cpgslct (id);
  cpglen (units, string, &qlen.val[0], &qlen.val[1]);
  GETDONESINGLE (units);
  GETDONESTRING (string);

  return (char *)&qlen;
}

//PGLINE -- draw a polyline (curve defined by line-segments)
char *
TkPgplot::Pgline (Value *args)
{
  GETSTART (2);
  GETFLOATARRAY (xpts);
  GETFLOATARRAY (ypts);
  cpgslct (id);
  cpgline (len, xpts, ypts);
  GETDONEARRAY (xpts);
  GETDONEARRAY (ypts);

  return "";
}

//PGMOVE -- move pen (change current pen position)
char *
TkPgplot::Pgmove (Value *args)
{
  GETSTART (2);
  GETFLOAT (x);
  GETFLOAT (y);
  cpgslct (id);
  cpgmove (x, y);
  GETDONESINGLE (x);
  GETDONESINGLE (y);

  return "";
}

//PGMTXT -- write text at position relative to viewport
char *
TkPgplot::Pgmtxt (Value *args)
{
  GETSTART (5);
  GETSTRING (side);
  GETFLOAT (disp);
  GETFLOAT (coord);
  GETFLOAT (fjust);
  GETSTRING (text);
  cpgslct (id);
  cpgmtxt (side, disp, coord, fjust, text);
  GETDONESTRING (side);
  GETDONESINGLE (disp);
  GETDONESINGLE (coord);
  GETDONESINGLE (fjust);
  GETDONESTRING (text);

  return "";
}

//PGNUMB -- convert a number into a plottable character string
char *
TkPgplot::Pgnumb (Value *args)
{
  GETSTART (3);
  GETINT (mm);
  GETINT (pp);
  GETINT (form);

  int numbSize = 80;		// Quite sufficient; looked at PGPLOT source.
  static char numb[80];

  cpgnumb (mm, pp, form, numb, &numbSize);
  GETDONESINGLE (mm);
  GETDONESINGLE (pp);
  GETDONESINGLE (form);

  return numb;
}

//PGOPEN -- open a graphics device
char *
TkPgplot::Pgopen (Value *args)
{
  GETSTART (1);
  GETSTRING (device);

  static tk_iarrayRec devno;

  devno.val = (int *)alloc_memory (sizeof (int));
  devno.len = 1;
  id = cpgopen (device);
  cpgslct (id);
  devno.val[0] = id;
  GETDONESTRING (device);

  return (char *)&devno;
}

//PGPAGE -- advance to new page
char *
TkPgplot::Pgpage (Value *args)
{
  cpgslct (id);
  cpgpage ();

  return "";
}

//PGPANL -- switch to a different panel on the view surface
char *
TkPgplot::Pgpanl (Value *args)
{
  GETSTART (2);
  GETINT (ix);
  GETINT (iy);
  cpgslct (id);
  cpgpanl (ix, iy);
  GETDONESINGLE (ix);
  GETDONESINGLE (iy);

  return "";
}

//PGPAP -- change the size of the view surface
char *
TkPgplot::Pgpap (Value *args)
{
  GETSTART (2);
  GETFLOAT (width);
  GETFLOAT (aspect);
  cpgslct (id);
  cpgpap (width, aspect);
  GETDONESINGLE (width);
  GETDONESINGLE (aspect);

  return "";
}

//PGPIXL -- draw pixels
char *
TkPgplot::Pgpixl (Value *args)
{
  GETSTART (5);
  GETINTARRAY (ia);
  GETSHAPE (ia);
  GETFLOAT (x1);
  GETFLOAT (x2);
  GETFLOAT (y1);
  GETFLOAT (y2);
  cpgslct (id);
  cpgpixl (ia, idim, jdim, 1, idim, 1, jdim, x1, x2, y1, y2);
  GETDONEARRAY (ia);
  GETDONESINGLE (x1);
  GETDONESINGLE (x2);
  GETDONESINGLE (y1);
  GETDONESINGLE (y2);

  return "";
}

//PGPNTS -- draw one or more graph markers, not all the same
char *
TkPgplot::Pgpnts (Value *args)
{
  GETSTART (3);
  GETFLOATARRAY (x);
  GETFLOATARRAY (y);

  int xy_len = len;

  GETINTARRAY (symbol);
  cpgslct (id);
  cpgpnts (xy_len, x, y, symbol, len);
  GETDONEARRAY (x);
  GETDONEARRAY (y);
  GETDONEARRAY (symbol);

  return "";
}

//PGPOLY -- fill a polygonal area with shading
char *
TkPgplot::Pgpoly (Value *args)
{
  GETSTART (2);
  GETFLOATARRAY (xpts);
  GETFLOATARRAY (ypts);
  cpgslct (id);
  cpgpoly (len, xpts, ypts);
  GETDONEARRAY (xpts);
  GETDONEARRAY (ypts);

  return "";
}

//PGPT -- draw one or more graph markers
char *
TkPgplot::Pgpt (Value *args)
{
  GETSTART (3);
  GETFLOATARRAY (xpts);
  GETFLOATARRAY (ypts);
  GETINT (symbol);
  cpgslct (id);
  cpgpt (len, xpts, ypts, symbol);
  GETDONEARRAY (xpts);
  GETDONEARRAY (ypts);
  GETDONESINGLE (symbol);

  return "";
}

//PGPTXT -- write text at arbitrary position and angle
char *
TkPgplot::Pgptxt (Value *args)
{
  GETSTART (5);
  GETFLOAT (x);
  GETFLOAT (y);
  GETFLOAT (angle);
  GETFLOAT (fjust);
  GETSTRING (text);
  cpgslct (id);
  cpgptxt (x, y, angle, fjust, text);
  GETDONESINGLE (x);
  GETDONESINGLE (y);
  GETDONESINGLE (angle);
  GETDONESINGLE (fjust);
  GETDONESTRING (text);

  return "";
}

//PGQAH -- inquire arrow-head style
char *
TkPgplot::Pgqah (Value *args)
{
  static tk_farrayRec qah;
  int fs = 0;			// Gets returned as float; can't mix types.

  qah.val  = (float *)alloc_memory (sizeof (float) * 3);
  qah.len = 3;
  cpgslct (id);
  cpgqah (&fs, &qah.val[1], &qah.val[2]);
  qah.val[0] = (float)fs;

  return (char *)&qah;
}

//PGQCF -- inquire character font
char *
TkPgplot::Pgqcf (Value *args)
{
  static tk_iarrayRec qcf;

  qcf.val = (int *)alloc_memory (sizeof (int));
  qcf.len = 1;
  cpgslct (id);
  cpgqcf (&qcf.val[0]);

  return (char *)&qcf;
}

//PGQCH -- inquire character height
char *
TkPgplot::Pgqch (Value *args)
{
  static tk_farrayRec qch;

  qch.val = (float *)alloc_memory (sizeof (float));
  qch.len = 1;
  cpgslct (id);
  cpgqch (&qch.val[0]);

  return (char *)&qch;
}

//PGQCI -- inquire color index
char *
TkPgplot::Pgqci (Value *args)
{
  static tk_iarrayRec qci;

  qci.val = (int *)alloc_memory (sizeof (int));
  qci.len = 1;
  cpgslct (id);
  cpgqci (&qci.val[0]);

  return (char *)&qci;
}

//PGQCIR -- inquire color index range
char *
TkPgplot::Pgqcir (Value *args)
{
  static tk_iarrayRec qcir;

  qcir.val = (int *)alloc_memory (sizeof (int) * 2);
  qcir.len = 2;
  cpgslct (id);
  cpgqcir (&qcir.val[0], &qcir.val[1]);

  return (char *)&qcir;
}

//PGQCOL -- inquire color capability
char *
TkPgplot::Pgqcol (Value *args)
{
  static tk_iarrayRec qcol;

  qcol.val = (int *)alloc_memory (sizeof (int) * 2);
  qcol.len = 2;
  cpgslct (id);
  cpgqcol (&qcol.val[0], &qcol.val[1]);

  return (char *)&qcol;
}

//PGQCR -- inquire color representation
char *
TkPgplot::Pgqcr (Value *args)
{
  GETSTART (1);
  GETINT (ci);

  static tk_farrayRec qcr;

  qcr.val = (float *)alloc_memory (sizeof (float) * 3);
  qcr.len = 3;
  cpgslct (id);
  cpgqcr (ci, &qcr.val[0], &qcr.val[1], &qcr.val[2]);
  GETDONESINGLE (ci);

  return (char *)&qcr;
}

//PGQCS -- inquire character height in a variety of units
char *
TkPgplot::Pgqcs (Value *args)
{
  GETSTART (1);
  GETINT (units);

  static tk_farrayRec qcs;

  qcs.val = (float *)alloc_memory (sizeof (float) * 2);
  qcs.len = 2;
  cpgslct (id);
  cpgqcs (units, &qcs.val[0], &qcs.val[1]);
  GETDONESINGLE (units);

  return (char *)&qcs;
}

//PGQFS -- inquire fill-area style
char *
TkPgplot::Pgqfs (Value *args)
{
  static tk_iarrayRec qfs;

  qfs.val = (int *)alloc_memory (sizeof (int));
  qfs.len = 1;
  cpgslct (id);
  cpgqfs (&qfs.val[0]);

  return (char *)&qfs;
}

//PGQHS -- inquire hatching style
char *
TkPgplot::Pgqhs (Value *args)
{
  static tk_farrayRec qhs;

  qhs.val = (float *)alloc_memory (sizeof (float) * 3);
  qhs.len = 3;
  cpgslct (id);
  cpgqhs (&qhs.val[0], &qhs.val[1], &qhs.val[2]);

  return (char *)&qhs;
}

//PGQID -- inquire current device identifier
char *
TkPgplot::Pgqid (Value *args)
{
  static tk_iarrayRec qid;

  qid.val = (int *)alloc_memory (sizeof (int));
  qid.len = 1;
  cpgslct (id);
  cpgqid (&qid.val[0]);

  return (char *)&qid;
}

//PGQINF -- inquire PGPLOT general information
char *
TkPgplot::Pgqinf (Value *args)
{
  GETSTART (1);
  GETSTRING (item);

  static char value[80];
  int length = 80;

  cpgslct (id);
  cpgqinf (item, value, &length);
  GETDONESTRING (item);

  return (char *)value;
}

//PGQITF -- inquire image transfer function
char *
TkPgplot::Pgqitf (Value *args)
{
  static tk_iarrayRec qitf;

  qitf.val = (int *)alloc_memory (sizeof (int));
  qitf.len = 1;
  cpgslct (id);
  cpgqitf (&qitf.val[0]);

  return (char *)&qitf;
}

//PGQLS -- inquire line style
char *
TkPgplot::Pgqls (Value *args)
{
  static tk_iarrayRec qls;

  qls.val = (int *)alloc_memory (sizeof (int));
  qls.len = 1;
  cpgslct (id);
  cpgqls (&qls.val[0]);

  return (char *)&qls;
}

//PGQLW -- inquire line width
char *
TkPgplot::Pgqlw (Value *args)
{
  static tk_iarrayRec qlw;

  qlw.val = (int *)alloc_memory (sizeof (int));
  qlw.len = 1;
  cpgslct (id);
  cpgqlw (&qlw.val[0]);

  return (char *)&qlw;
}

//PGQPOS -- inquire current pen position
char *
TkPgplot::Pgqpos (Value *args)
{
  static tk_farrayRec qpos;

  qpos.val = (float *)alloc_memory (sizeof (float) * 2);
  qpos.len = 2;
  cpgslct (id);
  cpgqpos (&qpos.val[0], &qpos.val[1]);

  return (char *)&qpos;
}

//PGQTBG -- inquire text background color index
char *
TkPgplot::Pgqtbg (Value *args)
{
  static tk_iarrayRec qtbg;

  qtbg.val = (int *)alloc_memory (sizeof (int));
  qtbg.len = 1;
  cpgslct (id);
  cpgqtbg (&qtbg.val[0]);

  return (char *)&qtbg;
}

//PGQTXT -- find bounding box of text string
char *
TkPgplot::Pgqtxt (Value *args)
{
  GETSTART (5);
  GETFLOAT (x);
  GETFLOAT (y);
  GETFLOAT (angle);
  GETFLOAT (fjust);
  GETSTRING (text);

  static tk_farrayRec qtxt;

  qtxt.val = (float *)alloc_memory (sizeof (float) * 8);
  qtxt.len = 8;
  cpgslct (id);
  cpgqtxt (x, y, angle, fjust, text, &qtxt.val[0], &qtxt.val[4]);
  GETDONESINGLE (x);
  GETDONESINGLE (y);
  GETDONESINGLE (angle);
  GETDONESINGLE (fjust);
  GETDONESTRING (text);

  return (char *)&qtxt;
}

//PGQVP -- inquire viewport size and position
char *
TkPgplot::Pgqvp (Value *args)
{
  GETSTART (1);
  GETINT (units);

  static tk_farrayRec qvp;

  qvp.val = (float *)alloc_memory (sizeof (float) * 4);
  qvp.len = 4;
  cpgslct (id);
  cpgqvp (units, &qvp.val[0], &qvp.val[1], &qvp.val[2], &qvp.val[3]);
  GETDONESINGLE (units);

  return (char *)&qvp;
}

//PGQVSZ -- find the window defined by the full view surface
char *
TkPgplot::Pgqvsz (Value *args)
{
  GETSTART (1);
  GETINT (units);

  static tk_farrayRec qvsz;

  qvsz.val = (float *)alloc_memory (sizeof (float) * 4);
  qvsz.len = 4;
  cpgslct (id);
  cpgqvsz (units, &qvsz.val[0], &qvsz.val[1], &qvsz.val[2], &qvsz.val[3]);
  GETDONESINGLE (units);

  return (char *)&qvsz;
}

//PGQWIN -- inquire window boundary coordinates
char *
TkPgplot::Pgqwin (Value *args)
{
  static tk_farrayRec qwin;

  qwin.val = (float *)alloc_memory (sizeof (float) * 4);
  qwin.len = 4;
  cpgslct (id);
  cpgqwin (&qwin.val[0], &qwin.val[1], &qwin.val[2], &qwin.val[3]);

  return (char *)&qwin;
}

//PGRECT -- draw a rectangle, using fill-area attributes
char *
TkPgplot::Pgrect (Value *args)
{
  GETSTART (4);
  GETFLOAT (x1);
  GETFLOAT (x2);
  GETFLOAT (y1);
  GETFLOAT (y2);
  cpgslct (id);
  cpgrect (x1, x2, y1, y2);
  GETDONESINGLE (x1);
  GETDONESINGLE (x2);
  GETDONESINGLE (y1);
  GETDONESINGLE (y2);

  return "";
}

//PGRND -- find the smallest 'round' nubmer greater than x
char *
TkPgplot::Pgrnd (Value *args)
{
  GETSTART (2);
  GETFLOAT (x);
  GETINT (nsub);

  static tk_farrayRec rnd;

  rnd.val = (float *)alloc_memory (sizeof (float));
  rnd.len = 1;
  rnd.val[0] = cpgrnd (x, &nsub);
  GETDONESINGLE (x);
  GETDONESINGLE (nsub);

  return (char *)&rnd;
}

//PGRNGE -- choose axis limits
char *
TkPgplot::Pgrnge (Value *args)
{
  GETSTART (2);
  GETFLOAT (x1);
  GETFLOAT (x2);

  static tk_farrayRec rnge;

  rnge.val = (float *)alloc_memory (sizeof (float) * 2);
  rnge.len = 2;
  cpgslct (id);
  cpgrnge (x1, x2, &rnge.val[0], &rnge.val[1]);
  GETDONESINGLE (x1);
  GETDONESINGLE (x2);

  return (char *)&rnge;
}

//PGSAH -- set arrow-head style
char *
TkPgplot::Pgsah (Value *args)
{
  GETSTART (3);
  GETINT (fs);
  GETFLOAT (angle);
  GETFLOAT (vent);
  cpgslct (id);
  cpgsah (fs, angle, vent);
  GETDONESINGLE (fs);
  GETDONESINGLE (angle);
  GETDONESINGLE (vent);

  return "";
}

//PGSAVE -- save PGPLOT attributes
char *
TkPgplot::Pgsave (Value *args)
{
  cpgslct (id);
  cpgsave ();

  return "";
}

//PGSCF -- set character font
char *
TkPgplot::Pgscf (Value *args)
{
  GETSTART (1);
  GETINT (font);
  cpgslct (id);
  cpgscf (font);
  GETDONESINGLE (font);

  return "";
}

//PGSCH -- set character height
char *
TkPgplot::Pgsch (Value *args)
{
  GETSTART (1);
  GETFLOAT (size);
  cpgslct (id);
  cpgsch (size);
  GETDONESINGLE (size);

  return "";
}

//PGSCI -- set color index
char *
TkPgplot::Pgsci (Value *args)
{
  GETSTART (1);
  GETINT (ci);
  cpgslct (id);
  cpgsci (ci);
  GETDONESINGLE (ci);

  return "";
}

//PGSCIR -- set color index range
char *
TkPgplot::Pgscir (Value *args)
{
  GETSTART (2);
  GETINT (icilo);
  GETINT (icihi);
  cpgslct (id);
  cpgscir (icilo, icihi);
  GETDONESINGLE (icilo);
  GETDONESINGLE (icihi);

  return "";
}

//PGSCR -- set color representation
char *
TkPgplot::Pgscr (Value *args)
{
  GETSTART (4);
  GETINT (ci);
  GETFLOAT (cr);
  GETFLOAT (cg);
  GETFLOAT (cb);
  cpgslct (id);
  cpgscr (ci, cr, cg, cb);
  GETDONESINGLE (ci);
  GETDONESINGLE (cr);
  GETDONESINGLE (cg);
  GETDONESINGLE (cb);

  return "";
}

//PGSCRN -- set color representation by name
char *
TkPgplot::Pgscrn (Value *args)
{
  GETSTART (2);
  GETINT (ci);
  GETSTRING (name);

  static tk_iarrayRec scrn;

  scrn.val = (int *)alloc_memory (sizeof (int));
  scrn.len = 1;
  cpgslct (id);
  cpgscrn (ci, name, &scrn.val[0]);
  GETDONESINGLE (ci);
  GETDONESTRING (name);

  return (char *)&scrn;
}

//PGSFS -- set fill-area style
char *
TkPgplot::Pgsfs (Value *args)
{
  GETSTART (1);
  GETINT (fs);
  cpgslct (id);
  cpgsfs (fs);
  GETDONESINGLE (fs);

  return "";
}

//PGSHLS -- set color representation using HLS system
char *
TkPgplot::Pgshls (Value *args)
{
  GETSTART (4);
  GETINT (ci);
  GETFLOAT (ch);
  GETFLOAT (cl);
  GETFLOAT (cs);
  cpgslct (id);
  cpgshls (ci, ch, cl, cs);
  GETDONESINGLE (ci);
  GETDONESINGLE (ch);
  GETDONESINGLE (cl);
  GETDONESINGLE (cs);

  return "";
}

//PGSHS -- set hatching style
char *
TkPgplot::Pgshs (Value *args)
{
  GETSTART (3);
  GETFLOAT (angle);
  GETFLOAT (sepn);
  GETFLOAT (phase);
  cpgslct (id);
  cpgshs (angle, sepn, phase);
  GETDONESINGLE (angle);
  GETDONESINGLE (sepn);
  GETDONESINGLE (phase);

  return "";
}

//PGSITF -- set image transfer function
char *
TkPgplot::Pgsitf (Value *args)
{
  GETSTART (1);
  GETINT (itf);
  cpgslct (id);
  cpgsitf (itf);
  GETDONESINGLE (itf);

  return "";
}

//PGSLCT -- select an open graphics device
char *
TkPgplot::Pgslct (Value *args)
{
  GETSTART (1);
  GETINT (ID);
  id = ID;
  cpgslct (id);
  GETDONESINGLE (ID);

  return "";
}

//PGSLS -- set line style
char *
TkPgplot::Pgsls (Value *args)
{
  GETSTART (1);
  GETINT (ls);
  cpgslct (id);
  cpgsls (ls);
  GETDONESINGLE (ls);

  return "";
}

//PGSLW -- set line width
char *
TkPgplot::Pgslw (Value *args)
{
  GETSTART (1);
  GETINT (lw);
  cpgslct (id);
  cpgslw (lw);
  GETDONESINGLE (lw);

  return "";
}

//PGSTBG -- set text background color index
char *
TkPgplot::Pgstbg (Value *args)
{
  GETSTART (1);
  GETINT (tbci);
  cpgslct (id);
  cpgstbg (tbci);
  GETDONESINGLE (tbci);

  return "";
}

//PGSUBP -- subdivide view surface into panels
char *
TkPgplot::Pgsubp (Value *args)
{
  GETSTART (2);
  GETINT (nxsub);
  GETINT (nysub);
  cpgslct (id);
  cpgsubp (nxsub, nysub);
  GETDONESINGLE (nxsub);
  GETDONESINGLE (nysub);

  return "";
}

//PGSVP -- set viewport (normalized device coordinates)
char *
TkPgplot::Pgsvp (Value *args)
{
  GETSTART (4);
  GETFLOAT (xleft);
  GETFLOAT (xright);
  GETFLOAT (ybot);
  GETFLOAT (ytop);
  cpgslct (id);
  cpgsvp (xleft, xright, ybot, ytop);
  GETDONESINGLE (xleft);
  GETDONESINGLE (xright);
  GETDONESINGLE (ybot);
  GETDONESINGLE (ytop);

  return "";
}

//PGSWIN -- set window
char *
TkPgplot::Pgswin (Value *args)
{
  GETSTART (4);
  GETFLOAT (x1);
  GETFLOAT (x2);
  GETFLOAT (y1);
  GETFLOAT (y2);
  cpgslct (id);
  cpgswin (x1, x2, y1, y2);
  GETDONESINGLE (x1);
  GETDONESINGLE (x2);
  GETDONESINGLE (y1);
  GETDONESINGLE (y2);

  return "";
}

//PGTBOX -- draw frame and write (DD) HH MM SS.S labelling
char *
TkPgplot::Pgtbox (Value *args)
{
  GETSTART (6);
  GETSTRING (xopt);
  GETFLOAT (xtick);
  GETINT (nxsub);
  GETSTRING (yopt);
  GETFLOAT (ytick);
  GETINT (nysub);
  cpgslct (id);
  cpgtbox (xopt, xtick, nxsub, yopt, ytick, nysub);
  GETDONESTRING (xopt);
  GETDONESINGLE (xtick);
  GETDONESINGLE (nxsub);
  GETDONESTRING (yopt);
  GETDONESINGLE (ytick);
  GETDONESINGLE (nysub);

  return "";
}

//PGTEXT -- write text (horizontal, left-justified)
char *
TkPgplot::Pgtext (Value *args)
{
  GETSTART (3);
  GETFLOAT (x);
  GETFLOAT (y);
  GETSTRING (text);
  cpgslct (id);
  cpgtext (x, y, text);
  GETDONESINGLE (x);
  GETDONESINGLE (y);
  GETDONESTRING (text);

  return "";
}

//PGUPDT -- update display
char *
TkPgplot::Pgupdt (Value *args)
{
  cpgslct (id);
  cpgupdt ();

  return "";
}

//PGUNSA -- restore PGPLOT attributes
char *
TkPgplot::Pgunsa (Value *args)
{
  cpgslct (id);
  cpgunsa ();

  return "";
}

//PGVECT -- vector map of a 2D data array, with blanking
char *
TkPgplot::Pgvect (Value *args)
{
  GETSTART (6);
  GETFLOATARRAY (a);
  GETFLOATARRAY (b);
  GETSHAPE (b);
  GETFLOAT (c_);
  GETINT (nc);
  GETFLOATARRAY (tr);
  GETFLOAT (blank);
  cpgslct (id);
  cpgvect (a, b, idim, jdim, 1, idim, 1, jdim, c_, nc, tr, blank);
  GETDONEARRAY (a);
  GETDONEARRAY (b);
  GETDONESINGLE (c_);
  GETDONESINGLE (nc);
  GETDONEARRAY (tr);
  GETDONESINGLE (blank);

  return "";
}

//PGVSIZ -- set viewport (inches)
char *
TkPgplot::Pgvsiz (Value *args)
{
  GETSTART (4);
  GETFLOAT (xleft);
  GETFLOAT (xright);
  GETFLOAT (ybot);
  GETFLOAT (ytop);
  cpgslct (id);
  cpgvsiz (xleft, xright, ybot, ytop);
  GETDONESINGLE (xleft);
  GETDONESINGLE (xright);
  GETDONESINGLE (ybot);
  GETDONESINGLE (ytop);

  return "";
}

//pgvstd -- set standard (default) viewport
char *
TkPgplot::Pgvstd (Value *args)
{
  cpgslct (id);
  cpgvstd ();

  return "";
}

//PGWEDG -- annotate an image plot with a wedge
char *
TkPgplot::Pgwedg (Value *args)
{
  GETSTART (6);
  GETSTRING (size);
  GETFLOAT (disp);
  GETFLOAT (width);
  GETFLOAT (fg);
  GETFLOAT (bg);
  GETSTRING (label);
  cpgslct (id);
  cpgwedg (size, disp, width, fg, bg, label);
  GETDONESTRING (size);
  GETDONESINGLE (disp);
  GETDONESINGLE (width);
  GETDONESINGLE (fg);
  GETDONESINGLE (bg);
  GETDONESTRING (label);

  return "";
}

//PGWNAD -- set window and adjust viewport to same aspect ratio
char *
TkPgplot::Pgwnad (Value *args)
{
  GETSTART (4);
  GETFLOAT (x1);
  GETFLOAT (x2);
  GETFLOAT (y1);
  GETFLOAT (y2);
  cpgslct (id);
  cpgwnad (x1, x2, y1, y2);
  GETDONESINGLE (x1);
  GETDONESINGLE (x2);
  GETDONESINGLE (y1);
  GETDONESINGLE (y2);

  return "";
}

void
TkPgplot::Create (ProxyStore *s, Value *args, void *)
{
  TkPgplot *ret;

  if ( args->Length() != 18 )
    InvalidNumberOfArgs(18);

  SETINIT
  SETVAL (parent, parent->IsAgentRecord ());
  SETDIM (width);
  SETDIM (height);
  SETVAL (region, region->IsNumeric ());
  SETVAL (axis, axis->IsNumeric ());
  SETVAL (nxsub, nxsub->IsNumeric ());
  SETVAL (nysub, nysub->IsNumeric ());
  SETSTR (relief);
  SETDIM (borderwidth);
  SETDIM (padx);
  SETDIM (pady);
  SETSTR (foreground);
  SETSTR (background);
  SETSTR (fill);
  SETINT (mincolor);
  SETINT (maxcolor);
  SETINT (cmap_share);
  SETINT (cmap_fail);

  TkAgent *agent = (TkAgent*) (global_store->GetProxy(parent));
  if (agent && !strcmp (agent->AgentID (), "<graphic:frame>")) {
    // pgplot likes to blurt out a bunch
    // of scroll events right off the bat...
    hold_glish_events++;
    ret = new TkPgplot (s, (TkFrame *)agent, width, height, region, axis,
			nxsub, nysub, relief, borderwidth, padx, pady, foreground,
			background, fill, mincolor, maxcolor, cmap_share, cmap_fail);
  } else {
    SETDONE
    global_store->Error("bad parent type");
    return;
  }

  CREATE_RETURN
  FlushGlishEvents();
}

const char **TkPgplot::PackInstruction () {
  static char *ret[5];
  int c = 0;

  if (fill) {
    ret[c++] = "-fill";
    ret[c++] = fill;

    if (!strcmp (fill, "both") || !strcmp (fill, frame->Expand ()) ||
	frame->NumChildren () == 1 && !strcmp (fill, "y")) {
      ret[c++] = "-expand";
      ret[c++] = "true";
    } else {
      ret[c++] = "-expand";
      ret[c++] = "false";
    }
    ret[c++] = 0;

    return (const char **)ret;
  } else {
    return 0;
  }
}

int TkPgplot::CanExpand () const {
  if (fill && (!strcmp (fill, "both") || !strcmp (fill, frame->Expand ()) ||
	       frame->NumChildren () == 1 && !strcmp (fill, "y"))) {
    return 1;
  }
  return 0;
}

// KeyPress, KeyRelease
void
TkPgplot::CursorEvent (const char *name, const char *type, const char *key,
		       int *device)
{
  float world[2];

  rvp_xwin2world (self, device[0], device[1], &world[0], &world[1]);

  recordptr rec = create_record_dict ();

  rec->Insert (strdup ("world"), new Value (world, 2, COPY_ARRAY));
  rec->Insert (strdup ("device"), new Value (device, 2, COPY_ARRAY));
  rec->Insert (strdup ("key"), new Value (key));
  rec->Insert (strdup ("type"), new Value (type));
  PostTkEvent (name, new Value (rec));
}

// ButtonPress, ButtonRelease
void
TkPgplot::CursorEvent (const char *name, const char *type, int button,
		       int *device)
{
  float world[2];

  rvp_xwin2world (self, device[0], device[1], &world[0], &world[1]);

  recordptr rec = create_record_dict ();

  rec->Insert (strdup ("world"), new Value (world, 2, COPY_ARRAY));
  rec->Insert (strdup ("device"), new Value (device, 2, COPY_ARRAY));
  rec->Insert (strdup ("button"), new Value (button));
  rec->Insert (strdup ("type"), new Value (type));
  PostTkEvent (name, new Value (rec));
}

// MotionNotify, LeaveNotify, EnterNotify
void
TkPgplot::CursorEvent (const char *name, const char *type, int *device)
{
  float world[2];

  rvp_xwin2world (self, device[0], device[1], &world[0], &world[1]);

  recordptr rec = create_record_dict ();

  rec->Insert (strdup ("world"), new Value (world, 2, COPY_ARRAY));
  rec->Insert (strdup ("device"), new Value (device, 2, COPY_ARRAY));
  rec->Insert (strdup ("type"), new Value (type));
  PostTkEvent (name, new Value (rec));
}

// Other XEvents
void
TkPgplot::CursorEvent (const char *name, const char *type)
{
  recordptr rec = create_record_dict ();

  rec->Insert (strdup ("type"), new Value (type));
  PostTkEvent (name, new Value (rec));
}

#endif
#endif