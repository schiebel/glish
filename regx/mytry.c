#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "regexp.h"

/*
**    !(a|b|c)*?[c-z]+           
**    acbbcbbcxzyt
**
**    !(abc|xyz)*?([x-z0-9]+)
**    abcabcxyz09
*/
PMOP pm;
main() {
    char *regex = strdup("\\S+?(\\d+)$");
    char buf[2048];
    char obuf[1024];
    int i;
    char *orig, *s_end, *s, *m;

    regexp *r = pregcomp(regex,regex+strlen(regex),&pm);
    gets( buf );
    while ( ! feof( stdin ) )
    	{
	if ( buf[0] == '#' )
	    {
	    regex = strdup( &buf[1] );
	    printf("new exp>\t%s\n",regex);
	    r = pregcomp(regex,regex+strlen(regex),&pm);
	    gets( buf );
	    continue;
	    }
        printf("exp>\t%s\n",regex);
        printf("buf>\t%s\n",buf);
	orig = s = buf;
	s_end = s + strlen(s);
	while ( s < s_end && pregexec(r, s, s_end, orig,1,0,1) )
	    {
	    if ( r->subbase && r->subbase != orig )
		{
		m = s;
		s = orig;
		orig = r->subbase;
		s = orig + (m - s);
		s_end = s + (s_end - m);
		}

	    if ( r->startp[0] && r->endp[0] )
		{
		strcpy(obuf, r->startp[0]);
	        printf("res>\t%s\n", obuf);
		}
	    else
		printf("err>\t0x%x\t0x%x\n",r->startp[0], r->endp[0]);

	    s = r->endp[0];
	    }
        printf("res>\t%d\n", 0);
	gets( buf );
	}
    return 0;
}
