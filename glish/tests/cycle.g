#####
##### OK
#####
#func one() {
#    public := [=]
#    public.frame := func() {
#        xxx := frame()
#        public := [=]
#        public.f := frame()
#        public.t := [=]
#        public.t.fun := func() { print 'hello' }
#        return public
#    }
#    return public
#}
#x := one()
#a := x.frame()
# a := F
# a := x.frame()
# a := F
# x := F
# x := one()
# a := x.frame()
# a := F

#####
##### OK
#####
#func two() {
#    public := [=]
#    public.frame := func() {
#        msg := 'hello'
#        f := frame()
#        public := [=]
#        public.f := frame()
#        public.fun := func() { print msg }
#        return public
#    }
#    return public
#}
#x := two()
##a := x.frame()
##f := a.fun
##f()
##a := F
##f()
##f := F
#
#####
##### OK
#####
#func three() {
#    public := [=]
#    public.frame := func() {
#        public := [=]
#        public.f := frame()
#        public.fun := func() { print 'hello' }
#        return ref public
#    }
#    return public
#}
#x := three()
##a := x.frame()
## a := F
#
#####
##### OK
#####
#func four( ) {
#    x := frame()
#    func f() { return pi }
#    return [=]
#}
##x := four()
#
#####
##### OK
#####
#func five( ) {
#    x := frame()
#    y := frame()
#    whenever x->* do
#        x := F
#    return [=]
#}
#x := five()
##<RESIZE LAST(??) FRAME>
#
#####
##### OK
#####
#func six( ) {
#    pub := [=]
#    pub.x := frame()
#    pub.y := frame()
#    whenever pub.x->* do
#        {
#        print 'here'
#        ##
#        ## the question is should this whenever
#        ## stop after this statement, currently
#        ## it doesn't...
#        ##
#        pub.x := F
#        }
#    return pub
#}
#x := six()
#x
##<RESIZE FIRST FRAME>
##x := F
##x := six()
##x := F
##<RESIZE FIRST FRAME>

#####
##### OK
#####
#func seven() {
#    public := [=]
#    public.frame := func() {
#        xxx := frame()
#        public := [=]
#        public.f := frame()
#        func fun() { print 'hello' }
#        return fun
#    }
#    return public
#}
#x := seven()
#f := x.frame()
#f := F

#####
##### OK
#####
#func eight( ) {
#    public := [=]
#    public.frame := func( ref rec ) {
#        xxx := frame()
#        rec.f := func() { print 'hello' }
#        return [=]
#    }
#    return public
#}
#x := eight()
#f := [=]
#x.frame(f)
#f
#f := F

#####
##### OK *** leave eight() uncommented when using this ***
#####
#func nine( EF, ref Z ) {
#    EF.frame(Z)
#}
#x := eight()
#f := [=]
#nine(x,f)
#f.f()
#f := F

#####
##### OK
#####
foobar := [=]
func ten( ) {
    private := [=]
    public := [=]
    private.xxx := frame()
    public.frame := func( ) {
        xxx := frame()
        ## Test both global and wider references,
        ## individually and together...
        global foobar
        foobar.f :=  func() { print 'hello' }
        wider private
        private.f := func() { print 'hello' }
        return [=]
    }
    return public
}
#x := ten()
#f := x.frame()
#f := F
#x := F
#foobar := F

#####
##### OK *** leave ten() uncommented when using this ***
#####
#func eleven( ) {
#    private := [=]
#    public := [=]
#    private.X := frame(background='red')
#    public.frame := func ( ) {
#        yyy := frame(background='blue')
#        xxx := ten( )
#        func f( ) { xxx.frame( ) }
#        return f
#    }
#    return public
#}
#x := eleven()
#f := x.frame()
#f()
#x := F
#f := F
#foobar := F

#####
##### OK
#####
#func twelve( ) {
#    public := [=]
#    private := [=]
#    private.frame := frame()
#    public.foo := func( ) { print "hello world!" }
#    return public
#}
#twelve().foo()

#####
##### OK *** frame doesn't disappear with this one ***
#####
#func thirteen( ) {
#    public := [=]
#    private := [=]
#    private.frame := frame()
#    public.foo := func( ) { print "hello world!" }
#    return ref public
#}
#tref := thirteen( )
#tref.foo()
#const tref := tref
#tref.foo()
