/* $Id$
** Copyright (c) 1997 Associated Universities Inc.
*/

#include "Glish/glish.h"
RCSID("@(#) $Id$")

#ifdef __alpha
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <machine/context.h>
#include <machine/fpu.h>
#include "system.h"

#define DIV(NAME,TYPE)							\
void NAME( TYPE *lhs, TYPE *rhs, int lhs_len, int rhs_incr )		\
	{								\
	int i,j;							\
	for ( i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )		\
		lhs[i] /= rhs[j];					\
	}

DIV(glish_fdiv,float)
DIV(glish_ddiv,double)

void glish_func_loop( double (*fn)( double ), double *lhs, double *arg, int len )
	{
	int i;
	for ( i=0; i < len; i++ )
		lhs[i] = (*fn)( arg[i] );
	}


extern int glish_abort_on_fpe;
extern int glish_sigfpe_trap;
void glish_sigfpe (int signal, siginfo_t *sig_code , struct sigcontext *uc_ptr)
	{
	unsigned long result,op1;

	glish_sigfpe_trap = 1;

	if ( glish_abort_on_fpe )
		{
		glish_cleanup( );
		fprintf(stderr,"\n[fatal error, 'floating point exception' (signal %d), exiting]\n", SIGFPE );
		install_signal_handler( SIGFPE, (signal_handler) SIG_DFL );
		kill( getpid(), SIGFPE );
		}

	if ( sig_code->si_code != FPE_FLTUND && sig_code->si_code == FPE_FLTINV )
		{
		/*
		** The format of fp operate instructions on the Alpha are:
		** |31       26|25    21|20    16|15          5|4    0|
		** |  Opcode   |   Fa   |   Fb   |   Function  |  Fc  |
		** Where operands are in Fa and Fb and result goes to Fc.
		*/
		result = uc_ptr->sc_fp_trigger_inst & 0x1fL; 
		op1 = (uc_ptr->sc_fp_trigger_inst & 0x1f0000L) >> 16; 

		if ( uc_ptr->sc_fpregs[op1] == 0x7ff0000000000000L )		/* check for +infinity */
			uc_ptr->sc_fpregs[result] = INT_MAX;
		else if ( uc_ptr->sc_fpregs[op1] == 0xfff0000000000000L )	/* check for -infinity */
			uc_ptr->sc_fpregs[result] = INT_MIN;
		else
			uc_ptr->sc_fpregs[result] = 0;

		uc_ptr->sc_pc += 4;
		}
	}

#endif
