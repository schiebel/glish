#include "Glish/Client.h"
#include "fortran.h"

extern "C" 
  {
      void FORTRAN_NAME(fr1)(int*,int *);
      int  FORTRAN_NAME(ff1)(int *);
  }

int main( int argc, char** argv )
	{
	Client c( argc, argv );

	for ( GlishEvent* e; (e = c.NextEvent()); )
		if ( e->Val()->IsNumeric() )
			{
			int arg = e->Val()->IntVal();
			int *result = (int*) malloc( sizeof(int) * 2 );
			FORTRAN_NAME(fr1)( &arg, result );
			result[1] = FORTRAN_NAME(ff1)( &arg );
			Value val( result, 2 );
			if ( e->IsRequest() )
				c.Reply( &val );
			else
				c.PostEvent( e->Name(), &val );
			}

	return 0;
	}
