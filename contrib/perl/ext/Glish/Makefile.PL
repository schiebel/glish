use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

sub MY::c_o {
'
.cc.o:
	c++ -DDEBUGGING -rdynamic -g -c $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $(INC) $*.cc
';
}

sub MY::xs_c { my $self = shift; $_=$self->MM::xs_c(); s/\.c/\.cc/g; $_ };
sub MY::xs_o { my $self = shift; $_=$self->MM::xs_o(); s/\.c/\.cc/g; $_ };
sub MY::clean { my $self = shift; $_=$self->MM::clean(); s/\.c/\.cc/g; $_ };
sub MY::constants { my $self = shift; $_=$self->MM::constants(); s/\.c/\.cc/g; $_ }

WriteMakefile(
    'NAME'	=> 'Glish',
    'VERSION'	=> '0.1',
    'LIBS'	=> ['-L/opt/local/share/lib/perl5.002/site_perl/sun4-solaris/auto/Glish -L/opt/SW4.0/SUNWspro/lib -lglish -lsds -lnpd -lC -lm -lnsl -lsocket'],
    'DEFINE'	=> '',
    'INC'	=> '-I/home/drs/dev/glish/include -I/home/drs/dev/glish/glish/include',
    'dynamic_lib' => {OTHERLDFLAGS => '-Wl,-rpath,/usr/lib/perl5/site_perl/5.005/i386-linux/auto/Glish:/usr/lib -L/usr/lib/perl5/site_perl/5.005/i386-linux/auto/Glish -lglish -lsos -lnpd -lnsl -lstdc++' },
    'XS'	=> { 'Glish.xs' => 'Glish.cc' },
);
