// $Id$
// Copyright (c) 1997 Associated Universities Inc.
#ifndef generic_h_
#define generic_h_

#if defined(__STDC__) || defined(__ANSI_CPP__) || defined(__hpux)

#define name2(x, y)             name2_sos(x, y)
#define name2_sos(x, y)         x##y
#define name3(x, y, z)          name3_sos(x, y, z)
#define name3_sos(x, y, z)      x##y##z
#define name4(w, x, y, z)       name4_sos(w, x, y, z)
#define name4_sos(w, x, y, z)   w##x##y##z

#else

#define name2(x, y)             x/**/y
#define name3(x, y, z)          x/**/y/**/z
#define name4(w, x, y, z)       w/**/x/**/y/**/z

#endif

#define declare(x, y)           name2(x, declare)(y)
#define implement(x, y)         name2(x, implement)(y)
#define declare2(x, y, z)       name2(x, declare2)(y, z)
#define implement2(x, y, z)     name2(x, implement2)(y, z)

#endif
