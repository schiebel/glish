// $Id$
// Copyright (c) 1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <string.h>
#include "Reporter.h"
#include "File.h"
#include <ctype.h>
#include "Glish/Stream.h"


File::File( const char *str_ ) : in(0), out(0), str(0), desc(0)
	{
	if ( !str_ || !str_[0] ) return;

	str = strdup(str_);

	int len = strlen(str);

	if ( str[0] == '<' )
		{
		type_ = IN;
		in = fopen( clean_string( ), "r" );
		}
	else if ( str[0] == '>' )
		{
		type_ = OUT;
		if ( str[1] == '>' )
			out = fopen( clean_string( ), "a" );
		else
			out = fopen( clean_string( ), "w" );
		}
	else if ( str[0] == '|' )
		{
		if ( str[len-1] == '|' )
			{
			type_ = PBOTH;
			dual_popen( clean_string( ), &in, &out ); 
			}
		else
			{
			type_ = POUT;
			out = popen( clean_string( ), "w" );
			}
		}
	else if ( str[len-1] == '|' )
		{
		type_ = PIN;
		in = popen( clean_string( ), "r" );
		}
	else
		type_ = ERR;
	}

char *File::read( )
	{
	if ( type_ != IN && type_ != PIN && type_ != PBOTH ||
	     ! in || feof(in) ) return 0;

	char buf[1025];
	buf[0] = '\0';

	fgets( buf, 1024, in );
	int l = strlen(buf);

	int len = l+1;
	char *ret = (char*) alloc_memory( len );
	memcpy( ret, buf, l+1 );

	while ( ! feof( in ) && ret[len-2] != '\n' )
		{
		fgets( buf, 1024, in );
		l = strlen(buf);
		ret = (char*) realloc_memory( ret, len + l );
		memcpy( &ret[len-1], buf, l+1 );
		len += l;
		}

	return ret;
	}

void File::write( charptr buf )
	{
	if ( type_ != OUT && type_ != POUT && type_ != PBOTH || ! out ) return;

	fwrite( buf, 1, strlen(buf), out );
	if ( type_ == PBOTH ) fflush( out );
	}

void File::close( Type t )
	{
	switch ( type_ )
	    {
	    case IN:
		if ( in ) fclose( in );
		in = 0;
		break;
	    case OUT:
		if ( out ) fclose( out );
		out = 0;
		break;
	    case PIN:
		if ( in ) pclose( in );
		in = 0;
		break;
	    case POUT:
		if ( out ) pclose( out );
		out = 0;
		break;
	    case PBOTH:
		if ( out && (t == PBOTH || t==POUT || t==OUT) )
			{
			dual_pclose( out );
			out = 0;
			}
		if ( in && (t == PBOTH || t==PIN || t==IN) )
			{
			dual_pclose( in );
			in = 0;
			}
		break;
	    }
	}
	
File::~File( )
	{
	if ( str ) free_memory( str );
	close();
	}

char *File::clean_string( )
	{
	static char *buffer = 0;
	static int buffer_len = 0;

	if ( ! buffer )
		{
		buffer_len = 1024;
		buffer = (char*) alloc_memory( 1024 );
		}

	const char *start = str;
	int len = strlen(str);
	const char *end = str + len - 1;

	if ( *start == '|' && *end == '|' ) --end;
	if ( *start == '>' ) { if ( *++start == '>' ) ++start; }
	else if ( *start == '|' || *start == '<' ) { ++start; }
	else if ( *end == '|' ) { --end; }

	while ( isspace(*start) ) ++start;
	while ( isspace(*end) ) --end;

	int ret_len = end-start+1;
	if ( ret_len+1 > buffer_len )
		{
		while ( ret_len+1 > buffer_len ) buffer_len += (int)(buffer_len*0.5);
		buffer = (char*) realloc_memory( buffer, buffer_len );
		}

	memcpy( buffer, start, ret_len );
	buffer[ret_len] = '\0';

	return buffer;
	}

void File::Describe( OStream& s ) const
	{
	s << "<FILE: " << str << ">";
	}

const char *File::Description( ) const
	{
	if ( desc ) return desc;

	((File*)this)->desc = (char*) alloc_memory( strlen(str) + 9);

	sprintf(((File*)this)->desc, "<FILE: %s>", str);

	return desc;
	}
