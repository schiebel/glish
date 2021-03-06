/*  $Id$
**
**  Main editing routines for editline library.
**
** Copyright (c) 1992,1993 Simmule Turner and Rich Salz.  All rights reserved.
** Copyright (c) 1997 Associated Universities Inc.    All rights reserved.
*/

#include "config.h"
#include "editline.h"
RCSID("@(#) $Id$")
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(_AIX)
#include <sys/select.h>
#endif

/*
**  Manifest constants.
*/
#define SCREEN_WIDTH	80
#define SCREEN_ROWS	24
#define NO_ARG		(-1)
#define DEL		127
#define CTL(x)		((x) & 0x1F)
#define ISCTL(x)	((x) && (x) < ' ')
#define UNCTL(x)	((x) + 64)
#define META(x)		((x) | 0x80)
#define ISMETA(x)	((x) & 0x80)
#define UNMETA(x)	((x) & 0x7F)
#if	!defined(HIST_SIZE)
#define HIST_SIZE	401
#endif	/* !defined(HIST_SIZE) */

typedef const CHAR	*STRING;

/*
**  Command status codes.
*/
typedef enum _STATUS {
    CSdone, CSeof, CSmove, CSdispatch, CSstay, CSsignal, CSnodata
} STATUS;

/*
**  The type of case-changing to perform.
*/
typedef enum _CASE {
    TOupper, TOlower
} CASE;

/*
**  Key to command mapping.
*/
typedef struct _KEYMAP {
    CHAR	Key;
    STATUS	(*Function)();
} KEYMAP;

/*
**  Command history structure.
*/
typedef struct _HISTORY {
    int		Size;
    int		Pos;
    CHAR	*Lines[HIST_SIZE];
} HISTORY;

/*
**  Globals.
*/
CHAR		editline_null_byte = '\0';
CHAR		*rl_data_incomplete = &editline_null_byte;

int		rl_eof;
int		rl_erase;
int		rl_intr;
int		rl_kill;
int		rl_quit;
#if	defined(DO_SIGTSTP) && defined(SIGTSTP)
int		rl_susp;
#endif	/* defined(DO_SIGTSTP) */
STATIC int	rl_nodata = 0x100;

STATIC CHAR		NIL[] = "";
STATIC char		NEWLINE[]= CRLF;
STATIC int		Signal;
FORWARD KEYMAP		Map[33];
FORWARD KEYMAP		MetaMap[17];
STATIC int		TTYwidth;
STATIC int		TTYrows;

/*
** Allow re-entrant editing
*/
typedef struct _EDITOR {
    STRING	Input;
    CHAR	*Line;
    const char	*Prompt;
    CHAR	*Yanked;
    char	*Screen;
    HISTORY	History;
    int		Repeat;
    int		End;
    int		Mark;
    int		Point;
    int		OldPoint;
    int		PushBack;
    int		Pushed;
    size_t	Length;
    size_t	ScreenCount;
    size_t	ScreenSize;
    char	*backspace;
    CHAR	still_collecting_data;
    CHAR	force_refresh;
} EDITOR;

STATIC EDITOR			default_editor;
STATIC EDITOR			*Editor = &default_editor;

#define Input			Editor->Input
#define Line			Editor->Line
#define Prompt			Editor->Prompt
#define Yanked			Editor->Yanked
#define Screen			Editor->Screen
#define History			Editor->History
#define Repeat			Editor->Repeat
#define End			Editor->End
#define Mark			Editor->Mark
#define Point			Editor->Point
#define OldPoint		Editor->OldPoint
#define PushBack		Editor->PushBack
#define Pushed			Editor->Pushed
#define Length			Editor->Length
#define ScreenCount		Editor->ScreenCount
#define ScreenSize		Editor->ScreenSize
#define backspace		Editor->backspace
#define still_collecting_data	Editor->still_collecting_data
#define force_refresh		Editor->force_refresh

/* Display print 8-bit chars as `M-x' or as the actual 8-bit char? */
int		rl_meta_chars = 1;

/*
**  Declarations.
*/
STATIC CHAR	*editinput();
STATIC CHAR	*nb_editinput();
#if	defined(USE_TERMCAP)
extern char	*getenv();
extern char	*tgetstr();
extern int	tgetent();
extern int	tgetnum();
#endif	/* defined(USE_TERMCAP) */

/*
**  TTY input/output functions.
*/

STATIC void
TTYflush()
{
    if (ScreenCount) {
	(void)write(1, Screen, ScreenCount);
	ScreenCount = 0;
    }
}

STATIC void
TTYput(c)
    const CHAR	c;
{
    Screen[ScreenCount] = c;
    if (++ScreenCount >= ScreenSize - 1) {
	ScreenSize += SCREEN_INC;
	Screen = realloc_char(Screen, ScreenSize);
    }
}

STATIC void
TTYputs(p)
    STRING	p;
{
    while (*p)
	TTYput(*p++);
}

STATIC void
TTYshow(c)
    CHAR	c;
{
    if (c == DEL) {
	TTYput('^');
	TTYput('?');
    }
    else if (ISCTL(c)) {
	TTYput('^');
	TTYput(UNCTL(c));
    }
    else if (rl_meta_chars && ISMETA(c)) {
	TTYput('M');
	TTYput('-');
	TTYput(UNMETA(c));
    }
    else
	TTYput(c);
}

STATIC void
TTYstring(p)
    CHAR	*p;
{
    while (*p)
	TTYshow(*p++);
}

STATIC unsigned int
TTYget()
{
    CHAR	c;

    TTYflush();
    if (Pushed) {
	Pushed = 0;
	return PushBack;
    }
    if (*Input)
	return *Input++;
    return read(0, &c, (size_t)1) == 1 ? c : EOF;
}

STATIC unsigned int
TTYnbget()
{
    CHAR	c;
    fd_set fdset;
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&fdset);
    FD_SET(0,&fdset);

    TTYflush();
    if (Pushed) {
	Pushed = 0;
	return PushBack;
    }
    if (*Input)
	return *Input++;

    if ( select(0+1,&fdset,0,0,&timeout) && FD_ISSET(0,&fdset) )
	return read(0, &c, (size_t)1) == 1 ? c : EOF;
    else
	return rl_nodata;
}

#define TTYback()	(backspace ? TTYputs((STRING)backspace) : TTYput('\b'))

STATIC void
TTYbackn(n)
    int		n;
{
    while (--n >= 0)
	TTYback();
}

STATIC void
TTYinfo()
{
    static int		init;
#if	defined(USE_TERMCAP)
    char		*term;
    char		buff[2048];
    char		*bp;
    char		*p;
#endif	/* defined(USE_TERMCAP) */
#if	defined(TIOCGWINSZ)
    struct winsize	W;
#endif	/* defined(TIOCGWINSZ) */

    if (init) {
#if	defined(TIOCGWINSZ)
	/* Perhaps we got resized. */
	if (ioctl(0, TIOCGWINSZ, &W) >= 0
	 && W.ws_col > 0 && W.ws_row > 0) {
	    TTYwidth = (int)W.ws_col;
	    TTYrows = (int)W.ws_row;
	}
#endif	/* defined(TIOCGWINSZ) */
	return;
    }
    init++;

    TTYwidth = TTYrows = 0;
#if	defined(USE_TERMCAP)
    bp = &buff[0];
    if ((term = getenv("TERM")) == NULL)
	term = "dumb";
    if (tgetent(buff, term) < 0) {
       TTYwidth = SCREEN_WIDTH;
       TTYrows = SCREEN_ROWS;
       return;
    }
    p = tgetstr("le", &bp);
    backspace = p ? string_dup(p) : NULL;
    TTYwidth = tgetnum("co");
    TTYrows = tgetnum("li");
#endif	/* defined(USE_TERMCAP) */

#if	defined(TIOCGWINSZ)
    if (ioctl(0, TIOCGWINSZ, &W) >= 0) {
	TTYwidth = (int)W.ws_col;
	TTYrows = (int)W.ws_row;
    }
#endif	/* defined(TIOCGWINSZ) */

    if (TTYwidth <= 0 || TTYrows <= 0) {
	TTYwidth = SCREEN_WIDTH;
	TTYrows = SCREEN_ROWS;
    }
}


/*
**  Print an array of words in columns.
*/
STATIC void
columns(ac, av)
    int		ac;
    CHAR	**av;
{
    CHAR	*p;
    int		i;
    int		j;
    int		k;
    int		len;
    int		skip;
    int		longest;
    int		cols;

    /* Find longest name, determine column count from that. */
    for (longest = 0, i = 0; i < ac; i++)
	if ((j = strlen((char *)av[i])) > longest)
	    longest = j;
    cols = TTYwidth / (longest + 3);

    TTYputs((STRING)NEWLINE);
    for (skip = ac / cols + 1, i = 0; i < skip; i++) {
	for (j = i; j < ac; j += skip) {
	    for (p = av[j], len = strlen((char *)p), k = len; --k >= 0; p++)
		TTYput(*p);
	    if (j + skip < ac)
		while (++len < longest + 3)
		    TTYput(' ');
	}
	TTYputs((STRING)NEWLINE);
    }
}

STATIC void
reposition()
{
    int		i;
    CHAR	*p;

    TTYput('\r');
    TTYputs((STRING)Prompt);
    for (i = Point, p = Line; --i >= 0; p++)
	TTYshow(*p);
}

STATIC void
left(Change)
    STATUS	Change;
{
    TTYback();
    if (Point) {
	if (ISCTL(Line[Point - 1]))
	    TTYback();
        else if (rl_meta_chars && ISMETA(Line[Point - 1])) {
	    TTYback();
	    TTYback();
	}
    }
    if (Change == CSmove)
	Point--;
}

STATIC void
right(Change)
    STATUS	Change;
{
    TTYshow(Line[Point]);
    if (Change == CSmove)
	Point++;
}

STATIC STATUS
ring_bell()
{
    TTYput('\07');
    TTYflush();
    return CSstay;
}

STATIC STATUS
do_macro(c)
    unsigned int	c;
{
    CHAR		name[4];

    name[0] = '_';
    name[1] = c;
    name[2] = '_';
    name[3] = '\0';

    if ((Input = (CHAR *)getenv((char *)name)) == NULL) {
	Input = NIL;
	return ring_bell();
    }
    return CSstay;
}

STATIC STATUS
do_forward(move)
    STATUS	move;
{
    int		i;
    CHAR	*p;

    i = 0;
    do {
	p = &Line[Point];
	for ( ; Point < End && (*p == ' ' || !isalnum(*p)); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	for (; Point < End && isalnum(*p); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	if (Point == End)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS
do_case(type)
    CASE	type;
{
    int		i;
    int		end;
    int		count;
    CHAR	*p;

    (void)do_forward(CSstay);
    if (OldPoint != Point) {
	if ((count = Point - OldPoint) < 0)
	    count = -count;
	Point = OldPoint;
	if ((end = Point + count) > End)
	    end = End;
	for (i = Point, p = &Line[i]; i < end; i++, p++) {
	    if (type == TOupper) {
		if (islower(*p))
		    *p = toupper(*p);
	    }
	    else if (isupper(*p))
		*p = tolower(*p);
	    right(CSmove);
	}
    }
    return CSstay;
}

STATIC STATUS
case_down_word()
{
    return do_case(TOlower);
}

STATIC STATUS
case_up_word()
{
    return do_case(TOupper);
}

STATIC void
ceol()
{
    int		extras;
    int		i;
    CHAR	*p;

    for (extras = 0, i = Point, p = &Line[i]; i <= End; i++, p++) {
	TTYput(' ');
	if (ISCTL(*p)) {
	    TTYput(' ');
	    extras++;
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    TTYput(' ');
	    TTYput(' ');
	    extras += 2;
	}
    }

    for (i += extras; i > Point; i--)
	TTYback();
}

STATIC void
clear_line()
{
    Point = -strlen(Prompt);
    TTYput('\r');
    ceol();
    Point = 0;
    End = 0;
    Line[0] = '\0';
}

STATIC STATUS
insert_string(p)
    CHAR	*p;
{
    size_t	len;
    int		i;
    CHAR	*new;
    CHAR	*q;

    len = strlen((char *)p);
    if (End + len >= Length) {
	if ((new = alloc_char(Length + len + MEM_INC)) == NULL)
	    return CSstay;
	if (Length) {
	    COPYFROMTO(new, Line, Length);
	    free_memory(Line);
	}
	Line = new;
	Length += len + MEM_INC;
    }

    for (q = &Line[Point], i = End - Point; --i >= 0; )
	q[len + i] = q[i];
    COPYFROMTO(&Line[Point], p, len);
    End += len;
    Line[End] = '\0';
    TTYstring(&Line[Point]);
    Point += len;

    return Point == End ? CSstay : CSmove;
}

STATIC STATUS
redisplay()
{
    TTYputs((STRING)NEWLINE);
    TTYputs((STRING)Prompt);
    TTYstring(Line);
    return CSmove;
}

STATIC STATUS
toggle_meta_mode()
{
    rl_meta_chars = !rl_meta_chars;
    return redisplay();
}


void *set_editor( void *Edit )
{
    EDITOR *old_edit = Editor;
    force_refresh = 1;
    Editor = (EDITOR*) Edit;
    return old_edit;
}

void *create_editor( )
{
    return alloc_zero_memory( sizeof(EDITOR) );
}

STATIC CHAR *
next_hist()
{
    return History.Pos >= History.Size - 1 ? NULL : History.Lines[++History.Pos];
}

STATIC CHAR *
prev_hist()
{
    return History.Pos == 0 ? NULL : History.Lines[--History.Pos];
}

STATIC STATUS
do_insert_hist(p)
    CHAR	*p;
{
    if (p == NULL)
	return ring_bell();
    Point = 0;
    reposition();
    ceol();
    End = 0;
    return insert_string(p);
}

STATIC STATUS
do_hist(move)
    CHAR	*(*move)();
{
    CHAR	*p;
    int		i;

    i = 0;
    do {
	if ((p = (*move)()) == NULL)
	    return ring_bell();
    } while (++i < Repeat);
    return do_insert_hist(p);
}

STATIC STATUS
h_next()
{
    return do_hist(next_hist);
}

STATIC STATUS
h_prev()
{
    return do_hist(prev_hist);
}

STATIC STATUS
h_first()
{
    return do_insert_hist(History.Lines[History.Pos = 0]);
}

STATIC STATUS
h_last()
{
    return do_insert_hist(History.Lines[History.Pos = History.Size - 1]);
}

/*
**  Return zero if pat appears as a substring in text.
*/
STATIC int
substrcmp(text, pat, len)
    char	*text;
    char	*pat;
    int		len;
{
    CHAR	c;

    if ((c = *pat) == '\0')
        return *text == '\0';
    for ( ; *text; text++)
        if (*text == c && strncmp(text, pat, len) == 0)
            return 0;
    return 1;
}

STATIC CHAR *
search_hist(search, move)
    CHAR	*search;
    CHAR	*(*move)();
{
    static CHAR	*old_search;
    int		len;
    int		pos;
    int		(*match)();
    char	*pat;

    /* Save or get remembered search pattern. */
    if (search && *search) {
	if (old_search)
	    free_memory(old_search);
	old_search = (CHAR *)string_dup((char *)search);
    }
    else {
	if (old_search == NULL || *old_search == '\0')
            return NULL;
	search = old_search;
    }

    /* Set up pattern-finder. */
    if (*search == '^') {
	match = strncmp;
	pat = (char *)(search + 1);
    }
    else {
	match = substrcmp;
	pat = (char *)search;
    }
    len = strlen(pat);

    for (pos = History.Pos; (*move)() != NULL; )
	if ((*match)((char *)History.Lines[History.Pos], pat, len) == 0)
            return History.Lines[History.Pos];
    History.Pos = pos;
    return NULL;
}

STATIC STATUS
h_search()
{
    static int	Searching;
    const char	*old_prompt;
    CHAR	*(*move)();
    CHAR	*p;

    if (Searching)
	return ring_bell();
    Searching = 1;

    clear_line();
    old_prompt = Prompt;
    Prompt = "Search: ";
    TTYputs((STRING)Prompt);
    move = Repeat == NO_ARG ? prev_hist : next_hist;
    p = editinput();
    Prompt = old_prompt;
    Searching = 0;
    TTYputs((STRING)Prompt);
    if (p == NULL && Signal > 0) {
	Signal = 0;
	clear_line();
	return redisplay();
    }
    p = search_hist(p, move);
    clear_line();
    if (p == NULL) {
	(void)ring_bell();
	return redisplay();
    }
    return do_insert_hist(p);
}

STATIC STATUS
fd_char()
{
    int		i;

    i = 0;
    do {
	if (Point >= End)
	    break;
	right(CSmove);
    } while (++i < Repeat);
    return CSstay;
}

STATIC void
save_yank(begin, i)
    int		begin;
    int		i;
{
    if (Yanked) {
	free_memory(Yanked);
	Yanked = NULL;
    }

    if (i < 1)
	return;

    if ((Yanked = alloc_char( i + 1 )) != NULL) {
	COPYFROMTO(Yanked, &Line[begin], i);
	Yanked[i] = '\0';
    }
}

STATIC STATUS
delete_string(count)
    int		count;
{
    int		i;
    CHAR	*p;

    if (count <= 0 || End == Point)
	return CSstay;

    if (count == 1 && Point == End - 1) {
	/* Optimize common case of delete at end of line. */
	End--;
	p = &Line[Point];
	i = 1;
	TTYput(' ');
	if (ISCTL(*p)) {
	    i = 2;
	    TTYput(' ');
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    i = 3;
	    TTYput(' ');
	    TTYput(' ');
	}
	TTYbackn(i);
	*p = '\0';
	return CSmove;
    }
    if (Point + count > End && (count = End - Point) <= 0)
	return CSstay;

    if (count > 1)
	save_yank(Point, count);

    for (p = &Line[Point], i = End - (Point + count) + 1; --i >= 0; p++)
	p[0] = p[count];
    ceol();
    End -= count;
    TTYstring(&Line[Point]);
    return CSmove;
}

STATIC STATUS
bk_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS
bk_del_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return delete_string(i);
}

STATIC STATUS
kill_line()
{
    int		i;

    if (Repeat != NO_ARG) {
	if (Repeat < Point) {
	    i = Point;
	    Point = Repeat;
	    reposition();
	    (void)delete_string(i - Point);
	}
	else if (Repeat > Point) {
	    right(CSmove);
	    (void)delete_string(Repeat - Point - 1);
	}
	return CSmove;
    }

    save_yank(Point, End - Point);
    Line[Point] = '\0';
    ceol();
    End = Point;
    return CSstay;
}

STATIC STATUS
insert_char(c)
    int		c;
{
    STATUS	s;
    CHAR	buff[2];
    CHAR	*p;
    CHAR	*q;
    int		i;

    if (Repeat == NO_ARG || Repeat < 2) {
	buff[0] = c;
	buff[1] = '\0';
	return insert_string(buff);
    }

    if ((p = alloc_char(Repeat + 1)) == NULL)
	return CSstay;
    for (i = Repeat, q = p; --i >= 0; )
	*q++ = c;
    *q = '\0';
    Repeat = 0;
    s = insert_string(p);
    free_memory(p);
    return s;
}

STATIC STATUS
meta()
{
    unsigned int	c;
    KEYMAP		*kp;

    if ((c = TTYget()) == EOF)
	return CSeof;
#if	defined(ANSI_ARROWS)
    /* Also include VT-100 arrows. */
    if (c == '[' || c == 'O')
	switch ((int)(c = TTYget())) {
	default:	return ring_bell();
	case EOF:	return CSeof;
	case 'A':	return h_prev();
	case 'B':	return h_next();
	case 'C':	return fd_char();
	case 'D':	return bk_char();
	}
#endif	/* defined(ANSI_ARROWS) */

    if (isdigit(c)) {
	for (Repeat = c - '0'; (c = TTYget()) != EOF && isdigit(c); )
	    Repeat = Repeat * 10 + c - '0';
	Pushed = 1;
	PushBack = c;
	return CSstay;
    }

    if (isupper(c))
	return do_macro(c);
    for (OldPoint = Point, kp = MetaMap; kp->Function; kp++)
	if (kp->Key == c)
	    return (*kp->Function)();

    return ring_bell();
}

STATIC STATUS
emacs(c)
    unsigned int	c;
{
    STATUS		s;
    KEYMAP		*kp;

    if (rl_meta_chars && ISMETA(c)) {
	Pushed = 1;
	PushBack = UNMETA(c);
	return meta();
    }
    for (kp = Map; kp->Function; kp++)
	if (kp->Key == c)
	    break;
    s = kp->Function ? (*kp->Function)() : insert_char((int)c);
    if (!Pushed)
	/* No pushback means no repeat count; hacky, but true. */
	Repeat = NO_ARG;
    return s;
}

STATIC STATUS
TTYspecial(c)
    unsigned int	c;
{
    if (ISMETA(c))
	return CSdispatch;

    if (c == rl_erase || c == DEL)
	return bk_del_char();
    if (c == rl_kill) {
	if (Point != 0) {
	    Point = 0;
	    reposition();
	}
	Repeat = NO_ARG;
	return kill_line();
    }
    if (c == rl_eof && Point == 0 && End == 0)
	return CSeof;
    if (c == rl_intr) {
	Signal = SIGINT;
	return CSsignal;
    }
    if (c == rl_quit) {
	Signal = SIGQUIT;
	return CSeof;
    }
#if	defined(DO_SIGTSTP) && defined(SIGTSTP)
    if (c == rl_susp) {
	Signal = SIGTSTP;
	return CSsignal;
    }
#endif	/* defined(DO_SIGTSTP) */
    if (c == rl_nodata)
	return CSnodata;

    return CSdispatch;
}

STATIC CHAR *
editinput()
{
    unsigned int	c;

    if (!still_collecting_data) {
	Repeat = NO_ARG;
	OldPoint = Point = Mark = End = 0;
	Line[0] = '\0';
    } else
	still_collecting_data = 0;

    Signal = -1;

    while ((c = TTYget()) != EOF)
	switch (TTYspecial(c)) {
	case CSdone:
	    return Line;
	case CSeof:
	    return NULL;
	case CSsignal:
	    return (CHAR *)"";
	case CSmove:
	    reposition();
	    break;
	case CSnodata:
	    return NULL;
	case CSdispatch:
	    switch (emacs(c)) {
	    case CSdone:
		return Line;
	    case CSeof:
		return NULL;
	    case CSsignal:
		return (CHAR *)"";
	    case CSmove:
		reposition();
		break;
	    case CSnodata:
	    case CSdispatch:
	    case CSstay:
		break;
	    }
	    break;
	case CSstay:
	    break;
	}
    return NULL;
}


STATIC CHAR *
nb_editinput()
{
    unsigned int	c;

    if (!still_collecting_data) {
	Repeat = NO_ARG;
	OldPoint = Point = Mark = End = 0;
	Line[0] = '\0';
    }
    Signal = -1;

    while ((c = TTYnbget()) != EOF)
	switch (TTYspecial(c)) {
	case CSdone:
	    still_collecting_data = 0;
	    return Line;
	case CSeof:
	    still_collecting_data = 0;
	    return NULL;
	case CSsignal:
	    still_collecting_data = 0;
	    return (CHAR *)"";
	case CSmove:
	    reposition();
	    break;
	case CSnodata:
	    still_collecting_data = 1;
	    return rl_data_incomplete;
	case CSdispatch:
	    switch (emacs(c)) {
	    case CSdone:
		still_collecting_data = 0;
		return Line;
	    case CSeof:
		still_collecting_data = 0;
		return NULL;
	    case CSsignal:
		still_collecting_data = 0;
		return (CHAR *)"";
	    case CSmove:
		reposition();
		break;
	    case CSnodata:
	    case CSdispatch:
	    case CSstay:
		break;
	    }
	    break;
	case CSstay:
	    break;
	}

    still_collecting_data = 0;
    return NULL;
}

STATIC void
hist_add(p)
    CHAR	*p;
{
    int		i;

    if ((p = (CHAR *)string_dup((char *)p)) == NULL)
	return;
    if (History.Size < HIST_SIZE)
	History.Lines[History.Size++] = p;
    else {
	free_memory(History.Lines[0]);
	for (i = 0; i < HIST_SIZE - 1; i++)
	    History.Lines[i] = History.Lines[i + 1];
	History.Lines[i] = p;
    }
    History.Pos = History.Size - 1;
}

/*
**  For compatibility with FSF readline.
*/
/* ARGSUSED0 */
void
rl_reset_terminal(p)
    char	*p;
{
}

void
rl_initialize()
{
}

/*
 * Manditory cleanup when finishing up a call to "readline()"
 * or a series of calls to "nb_readline()"
 */ 
STATIC void
readline_cleanup()
{
    rl_ttyset(1);
    free_memory(Screen);
    free_memory(History.Lines[--History.Size]);
}

char *
readline(prompt)
    const char	*prompt;
{
    CHAR	*line;
    int		s;

    if ( Input == NULL ) Input = NIL;

    if (Line == NULL) {
	Length = MEM_INC;
	if ((Line = alloc_char(Length)) == NULL)
	    return NULL;
    }

    if (!still_collecting_data) {
	TTYinfo();
	rl_ttyset(0);
	hist_add(NIL);
	ScreenSize = SCREEN_INC;
	Prompt = prompt ? prompt : (char *)NIL;
	Screen = alloc_char(ScreenSize);
	TTYputs((STRING)Prompt);
    }

    if ((line = editinput()) != NULL) {
	line = (CHAR *)string_dup((char *)line);
	TTYputs((STRING)NEWLINE);
	TTYflush();
    }
    readline_cleanup();
    if (Signal > 0) {
	s = Signal;
	Signal = 0;
	(void)kill( 0, s);
    }
    return (char *)line;
}

/*
 * Necessary since terminal is left in a corrupt state
 * between calls to "nb_readline()". "still_collecting_data"
 * is used to tell if the tty needs reset or not.
 */
void
nb_readline_cleanup()
{
    if (still_collecting_data)
	{
	still_collecting_data = 0;
	readline_cleanup();
	}
}

/*
 * Somtimes it is necessary to reset the terminal state because
 * there will be an interruption in calls to nb_readline. This
 * serves that purpose.
 */
void
nb_reset_term( int enter )
	{
	static unsigned int count = 0;
	if ( enter )
		{
		if ( ! count && still_collecting_data )
			rl_ttyset(1);
		count += 1;
		}
	else if ( count )
		{
		count -= 1;
		if ( ! count && still_collecting_data )
			{
			rl_ttyset(0);
			/*
			 * delay refresh until more characters are read
			 * (i.e. "nb_readline()" is called) to avoid
			 * "loading up" the buffer.
			 */
			force_refresh = 1;
			}
		}
	}

char *
nb_readline(prompt)
    const char	*prompt;
{
    CHAR	*line;
    int		s;

    if ( Input == NULL ) Input = NIL;

    if (Line == NULL) {
	Length = MEM_INC;
	if ((Line = alloc_char(Length)) == NULL)
	    return NULL;
    }

    if (!still_collecting_data) {
	force_refresh = 0;
	TTYinfo();
	rl_ttyset(0);
	hist_add(NIL);
	ScreenSize = SCREEN_INC;
	Prompt = prompt ? prompt : (char *)NIL;
	Screen = alloc_char(ScreenSize);
	TTYputs((STRING)Prompt);
    }

    if ( force_refresh ) {
	force_refresh = 0;

	TTYput('\r');

	/* do these two instead of redisplay()
	 * to avoid generating a new line
	 */
	TTYputs((STRING)Prompt);
	TTYstring(Line);
    }


    if ((line = nb_editinput()) != NULL && line != rl_data_incomplete) {
	line = (CHAR *)string_dup((char *)line);
	TTYputs((STRING)NEWLINE);
	TTYflush();
    }
    if ( !still_collecting_data || Signal > 0 ) {
	still_collecting_data = 0;
	readline_cleanup();
    }

    if (Signal > 0) {
	s = Signal;
	Signal = 0;
	(void)kill( 0, s);
    }
    return (char *)line;
}

void
add_history(p)
    char	*p;
{
    if (p == NULL || *p == '\0')
	return;

#if	defined(UNIQUE_HISTORY)
    if (History.Size && strcmp(p, History.Lines[History.Size - 1]) == 0)
        return;
#endif	/* defined(UNIQUE_HISTORY) */
    hist_add((CHAR *)p);
}


STATIC STATUS
beg_line()
{
    if (Point) {
	Point = 0;
	return CSmove;
    }
    return CSstay;
}

STATIC STATUS
del_char()
{
    return delete_string(Repeat == NO_ARG ? 1 : Repeat);
}

STATIC STATUS
end_line()
{
    if (Point != End) {
	Point = End;
	return CSmove;
    }
    return CSstay;
}

/*
**  Move back to the beginning of the current word and return an
**  allocated copy of it.
*/
STATIC CHAR *
find_word()
{
    static char	SEPS[] = "#;&|^$=`'{}()<>\n\t ";
    CHAR	*p;
    CHAR	*new;
    size_t	len;

    for (p = &Line[Point]; p > Line && strchr(SEPS, (char)p[-1]) == NULL; p--)
	continue;
    len = Point - (p - Line) + 1;
    if ((new = alloc_char(len)) == NULL)
	return NULL;
    COPYFROMTO(new, p, len);
    new[len - 1] = '\0';
    return new;
}

/***
STATIC STATUS
c_complete()
{
    CHAR	*p;
    CHAR	*word;
    int		unique;
    STATUS	s;

    word = find_word();
    p = (CHAR *)rl_complete((char *)word, &unique);
    if (word)
	free_memory(word);
    if (p && *p) {
	s = insert_string(p);
	if (!unique)
	    (void)ring_bell();
	free_memory(p);
	return s;
    }
    return ring_bell();
}
***/

STATIC STATUS
c_possible()
{
    CHAR	**av;
    CHAR	*word;
    int		ac;

    word = find_word();
    ac = rl_list_possib((char *)word, (char ***)&av);
    if (word)
	free_memory(word);
    if (ac) {
	columns(ac, av);
	while (--ac >= 0)
	    free_memory(av[ac]);
	free_memory(av);
	return CSmove;
    }
    return ring_bell();
}

STATIC STATUS
accept_line()
{
    Line[End] = '\0';
    return CSdone;
}

STATIC STATUS
transpose()
{
    CHAR	c;

    if (Point) {
	if (Point == End)
	    left(CSmove);
	c = Line[Point - 1];
	left(CSstay);
	Line[Point - 1] = Line[Point];
	TTYshow(Line[Point - 1]);
	Line[Point++] = c;
	TTYshow(c);
    }
    return CSstay;
}

STATIC STATUS
quote()
{
    unsigned int	c;

    return (c = TTYget()) == EOF ? CSeof : insert_char((int)c);
}

STATIC STATUS
wipe()
{
    int		i;

    if (Mark > End)
	return ring_bell();

    if (Point > Mark) {
	i = Point;
	Point = Mark;
	Mark = i;
	reposition();
    }

    return delete_string(Mark - Point);
}

STATIC STATUS
mk_set()
{
    Mark = Point;
    return CSstay;
}

STATIC STATUS
exchange()
{
    unsigned int	c;

    if ((c = TTYget()) != CTL('X'))
	return c == EOF ? CSeof : ring_bell();

    if ((c = Mark) <= End) {
	Mark = Point;
	Point = c;
	return CSmove;
    }
    return CSstay;
}

STATIC STATUS
yank()
{
    if (Yanked && *Yanked)
	return insert_string(Yanked);
    return CSstay;
}

STATIC STATUS
copy_region()
{
    if (Mark > End)
	return ring_bell();

    if (Point > Mark)
	save_yank(Mark, Point - Mark);
    else
	save_yank(Point, Mark - Point);

    return CSstay;
}

STATIC STATUS
move_to_char()
{
    unsigned int	c;
    int			i;
    CHAR		*p;

    if ((c = TTYget()) == EOF)
	return CSeof;
    for (i = Point + 1, p = &Line[i]; i < End; i++, p++)
	if (*p == c) {
	    Point = i;
	    return CSmove;
	}
    return CSstay;
}

STATIC STATUS
fd_word()
{
    return do_forward(CSmove);
}

STATIC STATUS
fd_kill_word()
{
    int		i;

    (void)do_forward(CSstay);
    if (OldPoint != Point) {
	i = Point - OldPoint;
	Point = OldPoint;
	return delete_string(i);
    }
    return CSstay;
}

STATIC STATUS
bk_word()
{
    int		i;
    CHAR	*p;

    i = 0;
    do {
	for (p = &Line[Point]; p > Line && !isalnum(p[-1]); p--)
	    left(CSmove);

	for (; p > Line && p[-1] != ' ' && isalnum(p[-1]); p--)
	    left(CSmove);

	if (Point == 0)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS
bk_kill_word()
{
    (void)bk_word();
    if (OldPoint != Point)
	return delete_string(OldPoint - Point);
    return CSstay;
}

STATIC int
argify(line, avp)
    CHAR	*line;
    CHAR	***avp;
{
    CHAR	*c;
    CHAR	**p;
    CHAR	**new;
    int		ac;
    int		i;

    i = MEM_INC;
    if ((*avp = p = (CHAR**) alloc_charptr(i))== NULL)
	 return 0;

    for (c = line; isspace(*c); c++)
	continue;
    if (*c == '\n' || *c == '\0')
	return 0;

    for (ac = 0, p[ac++] = c; *c && *c != '\n'; ) {
	if (isspace(*c)) {
	    *c++ = '\0';
	    if (*c && *c != '\n') {
		if (ac + 1 == i) {
		    new = (CHAR**) alloc_charptr(i + MEM_INC);
		    if (new == NULL) {
			p[ac] = NULL;
			return ac;
		    }
		    COPYFROMTO(new, p, i * sizeof (char **));
		    i += MEM_INC;
		    free_memory(p);
		    *avp = p = new;
		}
		p[ac++] = c;
	    }
	}
	else
	    c++;
    }
    *c = '\0';
    p[ac] = NULL;
    return ac;
}

STATIC STATUS
last_argument()
{
    CHAR	**av;
    CHAR	*p;
    STATUS	s;
    int		ac;

    if (History.Size == 1 || (p = History.Lines[History.Size - 2]) == NULL)
	return ring_bell();

    if ((p = (CHAR *)string_dup((char *)p)) == NULL)
	return CSstay;
    ac = argify(p, &av);

    if (Repeat != NO_ARG)
	s = Repeat < ac ? insert_string(av[Repeat]) : ring_bell();
    else
	s = ac ? insert_string(av[ac - 1]) : CSstay;

    if (ac)
	free_memory(av);
    free_memory(p);
    return s;
}

static char *the_history_file = 0;
void initialize_readline_history( const char *history_file )
{
    char buf[2048], *s;
    FILE *f = 0;
    int len;
    if ( ! the_history_file ) {
	the_history_file = strdup(history_file);
	f = fopen( history_file,  "r" );
	if ( ! f ) return;
	while ( ! feof(f) ) {
	    s = fgets( buf, 2048, f );
	    if ( s ) {
	        len = strlen( s );
		if ( len > 0 && s[len-1] == '\n' )
		    s[len-1] = '\0';
	    	hist_add( s );
	    }
	}
	fclose( f );
	the_history_file = strdup(history_file);
    }
}
void finalize_readline_history( )
{
    int i = 0;
    FILE *f = 0;
    if ( the_history_file ) {
        if ( History.Size < HIST_SIZE )
	    f = fopen( the_history_file, "w" );
	else
	    f = fopen( the_history_file, "a" );

	if ( f ) {
	    for ( i=0; i < History.Size; ++i ) {
	        if ( i < History.Size-1 || strcmp( History.Lines[i], "exit" ) )
		    {
		    fputs( History.Lines[i], f );
		    fputc( '\n', f );
		    }
	    }
	    fclose( f );
	}
    }
}

STATIC KEYMAP	Map[33] = {
    {	CTL('@'),	ring_bell	},
    {	CTL('A'),	beg_line	},
    {	CTL('B'),	bk_char		},
    {	CTL('D'),	del_char	},
    {	CTL('E'),	end_line	},
    {	CTL('F'),	fd_char		},
    {	CTL('G'),	ring_bell	},
    {	CTL('H'),	bk_del_char	},
    {	CTL('I'),	ring_bell	},
/*  {	CTL('I'),	c_complete	}, */
    {	CTL('J'),	accept_line	},
    {	CTL('K'),	kill_line	},
    {	CTL('L'),	redisplay	},
    {	CTL('M'),	accept_line	},
    {	CTL('N'),	h_next		},
    {	CTL('O'),	ring_bell	},
    {	CTL('P'),	h_prev		},
    {	CTL('Q'),	ring_bell	},
    {	CTL('R'),	h_search	},
    {	CTL('S'),	ring_bell	},
    {	CTL('T'),	transpose	},
    {	CTL('U'),	ring_bell	},
    {	CTL('V'),	quote		},
    {	CTL('W'),	wipe		},
    {	CTL('X'),	exchange	},
    {	CTL('Y'),	yank		},
#if	! defined(DO_SIGTSTP) || ! defined(SIGTSTP)
    {	CTL('Z'),	ring_bell	},
#endif
    {	CTL('['),	meta		},
    {	CTL(']'),	move_to_char	},
    {	CTL('^'),	ring_bell	},
    {	CTL('_'),	ring_bell	},
    {	0,		NULL		}
};

STATIC KEYMAP	MetaMap[17]= {
    {	CTL('H'),	bk_kill_word	},
    {	DEL,		bk_kill_word	},
    {	' ',		mk_set		},
    {	'.',		last_argument	},
    {	'<',		h_first		},
    {	'>',		h_last		},
    {	'?',		c_possible	},
    {	'b',		bk_word		},
    {	'd',		fd_kill_word	},
    {	'f',		fd_word		},
    {	'l',		case_down_word	},
    {	'm',		toggle_meta_mode},
    {	'u',		case_up_word	},
    {	'y',		yank		},
    {	'w',		copy_region	},
    {	0,		NULL		}
};
