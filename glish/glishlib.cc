// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")

#include "Glish/Value.h"
#include "Reporter.h"
#include "glishlib.h"

DEFINE_CREATE_VALUE(Value)

const Value *lookup_sequencer_value( const char *id )
	{
	return 0;
	}

Value *copy_value( const Value *value )
	{
	if ( value->IsRef() )
		return copy_value( value->RefPtr() );

	Value *copy = 0;
	switch( value->Type() )
		{
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
		case TYPE_COMPLEX:
		case TYPE_DCOMPLEX:
		case TYPE_STRING:
		case TYPE_RECORD:
			copy = create_value( *value );
			break;

		case TYPE_OPAQUE:
			{
			// _AIX requires a temporary
			SDS_Index tmp(value->SDS_IndexVal());
			copy = create_value( tmp );
			copy->CopyAttributes( value );
			}
			break;

		case TYPE_SUBVEC_REF:
			switch ( value->VecRefPtr()->Type() )
				{
#define COPY_REF(tag,accessor)						\
	case tag:							\
		copy = create_value( value->accessor ); 		\
		copy->CopyAttributes( value );				\
		break;

				COPY_REF(TYPE_BOOL,BoolRef())
				COPY_REF(TYPE_BYTE,ByteRef())
				COPY_REF(TYPE_SHORT,ShortRef())
				COPY_REF(TYPE_INT,IntRef())
				COPY_REF(TYPE_FLOAT,FloatRef())
				COPY_REF(TYPE_DOUBLE,DoubleRef())
				COPY_REF(TYPE_COMPLEX,ComplexRef())
				COPY_REF(TYPE_DCOMPLEX,DcomplexRef())
				COPY_REF(TYPE_STRING,StringRef())

				default:
					fatal->Report(
						"bad type in copy_value()" );
				}
			break;

		default:
			fatal->Report( "bad type in copy_value()" );
		}

	return copy;
	}
