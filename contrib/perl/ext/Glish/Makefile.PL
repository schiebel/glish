use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

sub MY::c_o {
'
.cc.o:
	CC -PIC -DDEBUGGING -Bdynamic -g -c $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $(INC) $*.cc
';
}

sub MY::xs_c { $_=MM->xs_c; s/\.c/\.cc/g; $_ };
sub MY::xs_o { $_=MM->xs_o; s/\.c/\.cc/g; $_ };
sub MY::clean { $_=MM->clean; s/\.c/\.cc/g; $_ };
sub MY::constants { $_=MM->constants; s/\.c/\.cc/g; $_ }

WriteMakefile(
    'NAME'	=> 'Glish',
    'VERSION'	=> '0.1',
    'LIBS'	=> ['-L/aips2/local/glish/lib/sun4sol.so -L/opt/SUNWspro/SC2.0.1 -lglish -lsds -lnpd -lC -lm -lnsl -lsocket'],
    'DEFINE'	=> '',
    'INC'	=> '-I/aips2/local/glish/include',
    'dynamic_lib' => {OTHERLDFLAGS => '-R /aips2/local/glish/lib/sun4sol.so:/opt/SUNWspro/SC2.0.1:/opt/local/lib:/usr/ucblib:/usr/ccs/lib:/usr/lib'},
    'XS'	=> { 'Glish.xs' => 'Glish.cc' }
);
