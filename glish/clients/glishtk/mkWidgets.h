// $Id$
// Copyright (c) 2002 Associated Universities Inc.
//
#ifndef mkwidgets_h_
#define mkwidgets_h_
#include "Glish/glishtk.h"
#include "Glish/Dict.h"

glish_declare(PDict,TkProxy);
typedef PDict(TkProxy) tkproxyhash;

class MkWidget : public TkProxy {
    public:
	MkWidget( ProxyStore *s );
    private:
	static int initialized;
};

class MkTab : public MkWidget {
    public:
	MkTab( ProxyStore *, TkFrame *, charptr width, charptr height );
	static void CreateContainer( ProxyStore *, Value * );
	static void CreateTab( ProxyStore *, Value * );
	void Raise( const char *tag );
	int WidgetCount( ) const { return count; }
	int ItemCount(const char *) const { return tabcount; }
	int NewItemCount(const char *) { return ++tabcount; }
	const char *Width( ) const { return width; }
	const char *Height( ) const { return height; }
	void Add( const char *tag, TkProxy *proxy );
	void Remove( const char *tag );
	void UnMap( );
	~MkTab( );
	ProxyStore *seq() { return store; }
    protected:
	char *width;
	char *height;
	static int count;
	int tabcount;
	tkproxyhash elements;
};

#endif
