mark_count := 0
func mark()
    {
    global mark_count
    mark_count +:= 1
    print '#', mark_count
    }

func assert( v, standard, slop=0.0 )
    {
    global mark_count
    if ( v > standard + slop ||
         v < standard - slop )
        {
        print "error test",mark_count,"failed,",v,"!=",standard
        }
    }
# ------------------------------------------------------------------
mark()
a := 900
func simple1()
    {
    a := 89
    return a
    }
assert(a,900)
assert(simple1(),89)
assert(a,900)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
b := 200
func warning1()
    {
    assert(b,200)
    b := 100
    return b
    }
assert(b,200)
assert(warning1(),100)
assert(b,200)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
b := 990
func simple2()
    {
    x := b
    global b := 100
    return x
    }
assert(b,990)
assert(simple2(),990)
assert(b,100)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
func simple3()
    {
    global a_new_var_1
    a_new_var_1 := 90
    loc := a_new_var_1
    a_new_var_1 *:= 2
    return loc
    }
assert(simple3(),90)
assert(a_new_var_1,180)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
count := 1
func x() {
  global count
  index := count
  count := count + 1
  ret := [=]
  ret.get := func () {
                x := index
                return x
             }
  return ret
}
a := x()
b := x()
c := x()
d := x()

assert(a.get(),1)
assert(b.get(),2)
assert(c.get(),3)
assert(d.get(),4)
assert(b.get(),2)
assert(a.get(),1)
assert(c.get(),3)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
a := 1
b := 2
func scope_trot()
    {
    a := 10
    b := 11
    global c := 12
    global d := 13
    global e_ := 14
    assert(a,10)
    assert(b,11)
    assert(c,12)
    assert(d,13)
    assert(e_,14)
    print '\t=> 1'
    {
        local b := 101
        local f := 102
        a := 103
        local d := 104
        e_ := 105
        assert(a,103)
        assert(b,101)
        assert(c,12)
        assert(d,104)
        assert(e_,105)
        assert(f,102)
        print '\t=> 2'
        {
            local f := 1001
            b := 1002
            g := 1003
            global h := 1004
            local e_ := 1006
            global d := 1007
            local i := 1008
            assert(a,103)
            assert(b,1002)
            assert(c,12)
            assert(d,1007)
            assert(e_,1006)
            assert(f,1001)
            assert(g,1003)
            assert(h,1004)
            assert(i,1008)
            print '\t=> 3'
        }
        assert(a,103)
        assert(b,1002)
        assert(c,12)
        assert(d,104)
        assert(e_,105)
        assert(f,102)
        assert(g,1003)
        assert(h,1004)
        assert(i,F)
        print '\t=> 4'
    }
    assert(a,103)
    assert(b,11)
    assert(c,12)
    assert(d,1007)
    assert(e_,105)
    assert(f,F)
    assert(g,1003)
    assert(h,1004)
    assert(i,F)
    print '\t=> 5'
    }
scope_trot()
assert(a,1)
assert(b,2)
assert(c,12)
assert(d,1007)
assert(e_,105)
assert(f,F)
assert(g,F)
assert(h,1004)
assert(i,F)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
func ctor(value)
    {
    ret := [=]
    self := [=]
    self.val := value
    ret.get := func () { return self.val }
    ret.put := func (v) { wider self; self.val := v }
    return ret
    }
a := ctor(190)
b := ctor(290)
c := ctor(390)
assert(a.get(),190)
assert(b.get(),290)
assert(c.get(),390)
a.put(200)
assert(a.get(),200)
assert(c.get(),390)
c.put(400)
assert(a.get(),200)
assert(c.get(),400)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
mark()
xx := func (v)
    {
    return v
    }
assert(xx(90),90)
# --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
exit
