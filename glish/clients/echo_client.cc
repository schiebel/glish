#include "Glish/glish.h"
#include "Glish/Client.h"

int main( int argc, char** argv )
	{
	Value ret( glish_true );
	Client c( argc, argv );

	for ( GlishEvent* e; (e = c.NextEvent()); ) ;

	c.PostEvent( "dtor", &ret );
	return 0;
	}
