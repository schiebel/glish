// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <iostream.h>
#include "Glish/Object.h"
#include "Glish/Value.h"
#include "Reporter.h"

name_list *glish_files = 0;
name_list *glish_desc = 0;

unsigned short file_name = 0;
unsigned short line_num = 0;

Str glish_errno( (const char*) "" );

const char* GlishObject::Description() const
	{
	return 0;
	}

void GlishObject::Describe( OStream& s ) const
	{
	DescribeSelf( s );
	}

int GlishObject::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	const char *d = Description();
	s << (d ? d : "<*unknown*>");
	return 1;
	}

Value *GlishObject::Fail( const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		) const
	{
	if ( file && glish_files )
		return generate_error( (*glish_files)[file], line, m0,m1,
				       m2,m3,m4,m5,m6,m7,m8,m9,
				       m10,m11,m12,m13,m14,m15,m16 );
	else
		return generate_error( m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,
				       m10,m11,m12,m13,m14,m15,m16 );
	}

const Str GlishObject::strFail( const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		) const
	{
	return generate_error_str( m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,
				   m10,m11,m12,m13,m14,m15,m16 );
	}

Value *GlishObject::Fail( const Value *v ) const
	{
	if ( v && v->Type() == TYPE_FAIL && file && glish_files )
		return create_value( v, (*glish_files)[file], line );
	else
		return Fail( RMessage(v) );
		
	}

Value *GlishObject::Fail( ) const
	{
	if ( file && glish_files )
		return error_value( 0, (*glish_files)[file], line );
	else
		return error_value( );
	}
