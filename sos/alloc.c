#include <stdlib.h>

void *npd_malloc( size_t size )
{
#if defined(_AIX) || defined(__alpha__)
        if ( ! size ) size += 8;
#endif
return malloc( size > 0 ? size : 8 );
}

void npd_free( void *ptr )
{
  if ( ptr ) free( ptr );
}
