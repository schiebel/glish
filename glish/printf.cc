// $Id$
//
// printf - duplicate the C library routine of the same name, but from
// the shell command level.
//
// This version by Chris Torek, based on an earlier version by Fred Blonder.
//
// Modifed by Darrell Schiebel to work with Glish (1998)...
// 	see http://www.cv.nrao.edu/glish/ for information about Glish.
// 
// Changes since Chris' and Fred's work:
// Copyright (c) 1998 Associated Universities Inc.
//
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include "printf.h"
#include <ctype.h>
#include <string.h>
#include <BuiltIn.h>

static char	*ctor(int,int);
static char	*doit( int, char*, char*, const_args_list*, int&, int, int, int, int);
static void	ctrl( char * );
static int	digit( char *, int );
static char	*illfmt( char*, char*, int, char* );

#define T_PRINTF	1
#define T_FPRINTF	2
#define T_SPRINTF	3

char *_do_gprintf( int type, char *format, const_args_list *args,
		   int arg_off, FILE *file, char *&buffer )
{
	register char *cp, *convp;
	register int ch, ndyn, flags;
	char cbuf[BUFSIZ];	// separates each conversion
	static char hasmod[] = "has integer length modifier";
	int index;
	char *err = 0;		// trap errors returned
	char ebuf[512];

	// find the minimum argument length
	int len = args->length();
	int elemlen = (*args)[arg_off]->Length();
	for ( int i=arg_off+1; i < len; ++i )
		if ( (*args)[i]->Length() < elemlen )
			elemlen = (*args)[i]->Length();

	// flags
#define	LONGF	1
#define	SHORTF	2

	ctrl(format);	// backslash interpretation of fmt string

	for ( int i=0; i < elemlen; ++i ) {

		cp = format;
		index = arg_off;

		// Scan format string for conversion specifications.
		// (The labels would be loops, but then everything falls
		// off the right.)
scan:
		while ((ch = *cp++) != '%') {
			if (ch == 0)
				goto loop;
			(void) putchar(ch);
		}

		ndyn = 0;
		flags = 0;
		convp = cbuf;
		*convp++ = ch;

		// scan for conversion character
cvt:
		switch (ch = *cp++) {

		case '\0':		// unterminated conversion
			continue;

		// string or character format
		case 'c': case 's':
			if (flags)
				return illfmt(cbuf, convp, ch, hasmod);
			if ( err=doit(type, cbuf, convp, args, index, i, ndyn, ch, ch) )
				return err;
			goto scan;

		// integer formats
			case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
			if ((flags & (LONGF|SHORTF)) == (LONGF|SHORTF))
				return illfmt(cbuf, convp, ch, "is both long and short");
			if ( err=doit(type, cbuf, convp, args, index, i, ndyn, ch,
				      flags & LONGF ? 'l' : flags & SHORTF ? 'h' : 'i') )
				return err;
			goto scan;

		// floating point formats
		case 'e': case 'E': case 'f': case 'g': case 'G':
			if (flags)
				return illfmt(cbuf, convp, ch, hasmod);
			if ( err=doit(type, cbuf, convp, args, index, i, ndyn, ch, 'f') )
				return err;
			goto scan;

		// Roman (well, why not?)
		case 'r': case 'R':
			if (flags)
				return illfmt(cbuf, convp, ch, hasmod);
			if ( err=doit(type, cbuf, convp, args, index, i, ndyn, 's', ch) )
				return err;
			goto scan;

		case '%':	// boring
			(void) putchar('%');
			goto scan;

		// short integers
		case 'h':
			flags |= SHORTF;
			break;

		// long integers
		case 'l':
			flags |= LONGF;
			break;

		// field-width or precision specifier, or flag: keep scanning
		case '.': case '#': case '-': case '+': case ' ':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			break;

		// dynamic field width or precision: count it
		case '*':
			ndyn++;
			break;

		default:	// something we cannot handle
			if (isascii(ch) && isprint(ch))
				sprintf( ebuf, "illegal conversion character `%c'", ch );
			else
				(void) sprintf(ebuf, "illegal conversion character `\\%03o'", (unsigned char)ch);
			return strdup(ebuf);
			// NOTREACHED
		}

		// 2 leaves room for ultimate conversion char and for \0
		if (convp >= &cbuf[sizeof(cbuf) - 2]) {
			return strdup("conversion string too long");
		}
		*convp++ = ch;
		goto cvt;
loop:		continue;
	}
	return 0;
}

static char *illfmt( char *cbuf, char *convp, int ch, char *why ) {
	char ebuf[512];
	*convp++ = ch;
	*convp = 0;
	sprintf(ebuf, "format `%s' illegal: %s", cbuf, why);
	return strdup(ebuf);
}

#define STRING(lhs)							\
{									\
	val = (*ap)[index];						\
	if ( val->Type() != TYPE_STRING )				\
		return strdup("bad type, non-string");			\
	lhs = (char*) strdup(val->StringPtr(0)[off]);			\
	++index;							\
}
#define INT(lhs)							\
{									\
	val = (*ap)[index];						\
	if ( ! val->IsNumeric() )					\
		return strdup("bad type, non-numeric");			\
	lhs = val->IntVal(off+1);					\
	++index;							\
}
#define SHORT(lhs)							\
{									\
	val = (*ap)[index];						\
	if ( ! val->IsNumeric() )					\
		return strdup("bad type, non-numeric");			\
	lhs = val->ShortVal(off+1);					\
	++index;							\
}
#define CHAR(lhs)							\
{									\
	val = (*ap)[index];						\
	if ( val->IsNumeric() )						\
		lhs = val->ByteVal(off+1);				\
	else if ( val->Type() == TYPE_STRING )				\
		{							\
		char *tstr;						\
		STRING(tstr)						\
		ctrl(tstr);						\
		lhs = *tstr;						\
		free( tstr );						\
		}							\
	else								\
		return strdup("bad type, non-numeric");			\
	++index;							\
}
#define FLOAT(lhs)							\
{									\
	val = (*ap)[index];						\
	if ( ! val->IsNumeric() )					\
		return strdup("bad type, non-numeric");			\
	lhs = val->FloatVal(off+1);					\
	++index;							\
}

// Emit a conversion.  cch holds the printf format character for
// this conversion; cty holds a simplified version (all integer
// conversions, e.g., are represented as 'i').
static char *doit( int type, char *cbuf, char *convp, const_args_list *ap,
		   int &index, int off, int ndyn, int cch, int cty)
{
	register char *s;
	const IValue *val;
	union {		// four basic conversion types
		int i;
		long l;
		double d;
		char *str;
	} arg;
	char *tsrt;
	int a1, a2;	// dynamic width and/or precision
	char ebuf[512];	// to report errors

	// finish off the conversion string
	s = convp;
	*s++ = cch;
	*s = 0;
	s = cbuf;

	// verify number of arguments
	if (index >= ap->length()) {
		sprintf( ebuf, "not enough args for format `%s'", s );
		return strdup(ebuf);
	}

	// pick up dynamic specifiers
	if (ndyn) {
		INT(a1)
		if (ndyn > 1)
			INT(a2)
		if (ndyn > 2) {
			sprintf( ebuf, "too many `*'s in `%s'", s );
			return strdup(ebuf);
		}
	}

#define	PRINTF(what)					\
	if (ndyn == 0)					\
		(void) printf(s, what);			\
	else if (ndyn == 1)				\
		(void) printf(s, a1, what);		\
	else						\
		(void) printf(s, a1, a2, what);

	// emit the appropriate conversion
	switch (cty) {

	// string
	case 's':
		STRING(arg.str)
		ctrl(arg.str);
		goto string;

	// roman (much like string)
	case 'r': case 'R':
		{
		int tmpi;
		INT(tmpi)
		arg.str = ctor(tmpi, cty == 'R');
		}
string:
		PRINTF(arg.str);
		break;

	// floating point
	case 'f':
		FLOAT(arg.d)
		PRINTF(arg.d);
		break;

	// character
	case 'c':
		CHAR(arg.i)
		goto integer;

	// short integer
	case 'h':
		SHORT(arg.i)
		goto integer;

	// integer
	case 'i':
		INT(arg.i)
integer:
		PRINTF(arg.i);
		break;

	// long integer
	case 'l':
		INT(arg.l)
		PRINTF(arg.l);
		break;
	}
	return 0;
}

// Return the index of the character c in the string s; character 0
// is NOT considered part of the string (unlike index() or strchr()).
// If c is not found (or is 0), return -1.
//
// This is used for hex and octal digit conversions in ctrl().
static int digit( char *s, register int c ) {
	register char *p;

	for (p = s; *p; p++)
		if (*p == c)
			return (p - s);
	return (-1);
}

//
// Convert backslash notation to control characters, in place.
//
static void ctrl( register char *s ) {
	register char *op = s;
	register int v, c;
	static char oct[] = "01234567";
	static char hex[] = "0123456789abcdefABCDEF";

	while ((c = *s++) != 0) {
		if (c != '\\') {
			*op++ = c;
			continue;
		}
		switch (*s++) {
		case '\0':	// end-of-string: user goofed
			s--;
			break;

		case '\\':	// backslash
			*op++ = '\\';
			break;

		case 'n':	// newline
			*op++ = '\n';
			break;

		case 't':	// horizontal tab
			*op++ = '\t';
			break;

		case 'r':	// carriage-return
			*op++ = '\r';
			break;

		case 'f':	// form-feed
			*op++ = '\f';
			break;

		case 'b':	// backspace
			*op++ = '\b';
			break;

		case 'v':	// vertical tab
			*op++ = '\13';
			break;

		case 'a':	// WARNING! DANGER! DANGER! DANGER!
			*op++ = '\7';
			break;

		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			// octal constant, 3 digits maximum
			v = digit(oct, s[-1]);
			if ((c = digit(oct, *s)) >= 0) {
				v = (v << 3) + c;
				if ((c = digit(oct, *++s)) >= 0) {
					v = (v << 3) + c;
					s++;
				}
			}
			*op++ = v;
			break;

		case 'x':	// hex constant
			v = 0;
			while ((c = digit(hex, *s)) >= 0) {
				if (c >= 16)
					c -= 6;
				v = (v << 4) + c;
				s++;
			}
			*op++ = v;
			break;

		// The name of this object is taken from troff:
		// \z might be better, but this has a precedent.
		// It exists solely so that we can end a hex constant
		// which must be followed by a legal hex character.
		case '&':	// special zero-width `character'
			break;

		default:
			*op++ = s[-1];
		}
	}
	*op = '\0';
}

//
// Convert integer to Roman Numerals. (How have you survived without it?)
//
static char *ctor( int x, int caps) {
	static char buf[BUFSIZ];
	register char *outp = buf;
	register unsigned n = x;
	register int u, v;
	register char *p, *q;

	if ((int)n < 0) {
		*outp++ = '-';
		n = -n;
	}
	p = caps ? "M\2D\5C\2L\5X\2V\5I" : "m\2d\5c\2l\5x\2v\5i";
	v = 1000;
	if (n >= v * BUFSIZ / 2)	// conservative
		return (strdup("[abortive Roman numeral]"));
	for (;;) {
		while (n >= v)
			*outp++ = *p, n -= v;
		if (n == 0)
			break;
		q = p + 1;
		u = v / *q;
		if (*q == 2)		// magic
			u /= *(q += 2);
		if (n + u >= v) {
			*outp++ = *++q;
			n += u;
		} else {
			p++;
			v /= *p++;
		}
	}
	*outp = 0;
	return (strdup(buf));
}
