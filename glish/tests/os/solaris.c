/*
**  On Solaris 2.*, integer division by zero generates an exception,
**  and it must be caught to continue. "sigfpe()" must be used for
**  this, i.e. neither "signal()" nor "sigaction()" will work.
*/

#include <siginfo.h>
#include <ucontext.h>
#include <signal.h>

void fpe_handler( int sig, siginfo_t *sip, ucontext_t *uap );

main()
	{
	int x, y, z;

	/*
	** use sigfpe(3) to setup handler
	*/
	sigfpe(FPE_INTDIV, fpe_handler);

	x = 2;
	y = 0;
	z = x / y;

	printf( "%d / %d = %d\n", x, y, z );
	exit(0);
	}

void fpe_handler( int sig, siginfo_t *sip, ucontext_t *uap )
	{
	/*
	**  increment program counter; sigfpe() (unlike "sigaction()"
	**  or "signal()") does not do this for you.
	*/
	uap->uc_mcontext.gregs[REG_PC] = uap->uc_mcontext.gregs[REG_nPC];
	}
