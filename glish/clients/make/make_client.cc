// $Id$
// Copyright (c) 1999 Associated Universities Inc.
//
// Glish "make" client - generates events for makefile actions.
//
// This client was built using the BSD make tool from NetBSD.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Glish/Client.h"
#include "make_client.h"

inline int streq( const char* a, const char* b )
        {
        return ! strcmp( a, b );
        }

#if 0
int main( int argc, char **argv ) {
    int ret = 0;
    char *foocmd[] = { "this is FOO #1", "this is FOO #2" };
    char *foodep[] = { "BAR", "BAZ" };
    char *barcmd[] = { "this is BAR", "${FOOBAR}" };
    char *bazcmd[] = { "this is BAZ" };
    char *suffcmd[] = { "making crap from bull: $@ $? $> $< $*" };
    char *cocmd[] = { "making o from c: $@ $? $> $< $*" };
    char *maintgts[] = { "FOO" };
    bMake_Init( argc, argv );
    bMake_Define( "FOOBAR", "foo and bar" );
    bMake_TargetDef( "FOO", foocmd, 2, foodep, 2 );
    bMake_TargetDef( "BAR", barcmd, 2, 0, 0 );
    bMake_TargetDef( "BAZ", bazcmd, 1, 0, 0 );
    bMake_SuffixDef( ".bull.crap", suffcmd, 1 );
    bMake_SuffixDef( ".c.o", cocmd, 1 );
    bMake_SetMain( maintgts, 1 );
    bMake( );
    bMake( );
    return bMake_Finish( );
}
#endif

int main( int argc, char** argv ) {
    Client c( argc, argv );
    bMake_Init( argc, argv );

    for ( GlishEvent* e; (e = c.NextEvent()); ) {
        Value *val = e->value;
        if ( streq( e->name, "variable" ) ) {
	    if ( val->Type() == TYPE_STRING && val->Length() > 0 ) {
	        char *name = val->StringVal();
	        bMake_Define( name, "1" );
	        free_memory(name);
	    } else if ( val->Type() == TYPE_RECORD && val->Length() >= 2 ) {
	        Value *name = val->NthField(1);
		Value *value = val->NthField(2);
		if ( name->Type() == TYPE_STRING && name->Length() > 0 ) {
		    char *n = name->StringVal();
		    char *v = name->StringVal();
		    bMake_Define( n, v );
		    free_memory(n);
		    free_memory(v);
		} else {
		    c.Error( "bad value for 'variable'" );
		    continue;
		}
	    } else { 
                c.Error( "bad value for 'variable'" );
		continue;
	    }
	} else if ( streq( e->name, "target" ) ) {
	    Value *tgt = 0;
	    Value *action = 0;
	    char *t = 0;
	    charptr *a = 0;
	    if ( val->Type() == TYPE_RECORD && val->Length() >= 2 &&
		 (tgt=val->NthField(1)) && tgt->Type() == TYPE_STRING && tgt->Length() > 0 &&
		 (action=val->NthField(2)) && action->Type() == TYPE_STRING && action->Length() > 0 &&
		 (t = tgt->StringVal()) && (a = action->StringPtr(0)) ) {
	        Value *dep;
		charptr *d;
	        if ( val->Length() == 2 ) {
		    bMake_TargetDef( t, a, action->Length(), 0, 0 );
		} else if ( (dep=val->NthField(3)) && dep->Type() == TYPE_STRING && dep->Length() > 0 &&
			    (d = dep->StringPtr(0)) ) {
		    bMake_TargetDef( t, a, action->Length(), d, dep->Length() );
		} else {
		    c.Error( "bad value for 'target'" );
		    continue;
		}
	    } else {
		c.Error( "bad value for 'target'" );
	        continue;
	    }
	} else if ( streq(e->name, "suffix" ) ) {
	    Value *suf = 0;
	    Value *action = 0;
	    char *s = 0;
	    charptr *a = 0;
	    if ( val->Type() == TYPE_RECORD && val->Length() >= 2 &&
		 (suf=val->NthField(1)) && suf->Type() == TYPE_STRING && suf->Length() > 0 &&
		 (action=val->NthField(2)) && action->Type() == TYPE_STRING && action->Length() > 0 &&
		 (s = suf->StringVal()) && (a = action->StringPtr(0)) ) {
	        bMake_SuffixDef( s, a, action->Length() );
	    } else {
		c.Error( "bad value for 'suffix'" );
	        continue;
	    }
	} else {
	    c.Error( "unknown event '%s'", e->name );
	    continue;
	}
    }

    bMake_Finish( );
    return 0;
}
