/*
**  This file demonstrates patching in results from the signal
**  handler for the alpha architecture. This is necessary for
**  casts of INF to integer.
*/
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <machine/context.h>
#include <machine/fpu.h>

void install_fp_handler (void);
void fpe_handler (int, siginfo_t *, struct sigcontext *);

void main (void)
	{
#define NUM 10
	double x[NUM] = { 23.871, 0.0/0.0, 15.8712, 1.0 / 0.0, 9012.981,
			  -0.0/0.0, 31.901, -1.0/0.0, 71.981, 8712.0817 };
	int y[NUM],i;

	double a, b, c;

	a = 3.0;
	b = 0.0;

	install_fp_handler();

	c = a / b;
	printf("before:\t%lg\n", c);

	ieee_set_fp_control(IEEE_TRAP_ENABLE_DZE);

	c = a / b;
	printf("after:\t%lg\n", c);

	printf("---- ---- ---- ---- ---- ---- ---- ---- ---- ----\n");

	printf( "before:\n" );
	printf( "------\n" );

	for ( i=0; i < NUM; i++ )
		y[i] = (int)x[i];
	for ( i=0; i < NUM; i++ )
		printf("%d\n",y[i]);

	ieee_set_fp_control(IEEE_TRAP_ENABLE_INV);

	printf( "\nafter:\n" );
	printf( "-----\n" );

	for ( i=0; i < NUM; i++ )
		y[i] = (int)x[i];
	for ( i=0; i < NUM; i++ )
		printf("%d\n",y[i]);
	}

void fpe_handler (int signal, siginfo_t *sig_code , struct sigcontext *uc_ptr)
	{
	double local_double;
	unsigned long result,op1,op2;

	if (sig_code->si_code != FPE_FLTUND)
		{
		switch (sig_code->si_code)
			{
		    case FPE_FLTDIV:
			fprintf (stderr, "\t\tfloating div by 0\n");
			fprintf (stderr, "\t\t-----------------\n");

			/*
			** The format of fp operate instructions on the Alpha are:
			** |31       26|25    21|20    16|15          5|4    0|
			** |  Opcode   |   Fa   |   Fb   |   Function  |  Fc  |
			** Where operands are in Fa and Fb and result goes to Fc.
			*/
			fprintf(stderr,"\t\tpc at fault: 0x%lx\n", uc_ptr->sc_pc);
			fprintf(stderr,"\t\tfaulting instruction: 0x%x\n", uc_ptr->sc_fp_trigger_inst);
			result = uc_ptr->sc_fp_trigger_inst & 0x1F; 
			fprintf(stderr,"\t\tresult register: %ld\n", result);
			local_double = 3.141592654;
			uc_ptr->sc_fpregs[result] = *((long *) &local_double);

			uc_ptr->sc_pc += 4;
			break;
		    case FPE_INTOVF:
			fprintf (stderr, "\t\tinteger overflow\n");
			fprintf (stderr, "\t\t----------------\n");
			uc_ptr->sc_pc += 4;
			break;
		    case FPE_INTDIV:
			fprintf (stderr, "\t\tinteger div by 0\n");
			fprintf (stderr, "\t\t--------------\n");
			uc_ptr->sc_pc += 4;
			break;
		    case FPE_FLTINV:
			fprintf (stderr, "\t\tinvalid float operation\n");
			fprintf (stderr, "\t\t-----------------------\n");

			/*
			** The format of fp operate instructions on the Alpha are:
			** |31       26|25    21|20    16|15          5|4    0|
			** |  Opcode   |   Fa   |   Fb   |   Function  |  Fc  |
			** Where operands are in Fa and Fb and result goes to Fc.
			*/
			fprintf(stderr,"\t\tpc at fault: 0x%lx\n", uc_ptr->sc_pc);
			fprintf(stderr,"\t\tfaulting instruction: 0x%x\n", uc_ptr->sc_fp_trigger_inst);
			result = uc_ptr->sc_fp_trigger_inst & 0x1fL; 
			fprintf(stderr,"\t\tresult register: %ld\n", result);
			op1 = (uc_ptr->sc_fp_trigger_inst & 0x1f0000L) >> 16; 
			fprintf(stderr,"\t\t1st operand register: %ld\n", op1);
			op2 = (uc_ptr->sc_fp_trigger_inst & 0x3e00000L) >> 21; 
			fprintf(stderr,"\t\t2nd operand register: %ld\n", op2);

			if ( uc_ptr->sc_fpregs[op1] == 0x7ff0000000000000L )
				uc_ptr->sc_fpregs[result] = INT_MAX;
			else if ( uc_ptr->sc_fpregs[op1] == 0xfff0000000000000L )
				uc_ptr->sc_fpregs[result] = INT_MIN;
			else
				uc_ptr->sc_fpregs[result] = 0;

			uc_ptr->sc_pc += 4;
			break;
		    default:                
			fprintf (stderr, "\t\tDidn't recognize error!\n");
			}
		}
	else
		fprintf (stderr, "Underflow Error \n");

	return;
	}

void install_fp_handler(void)
	{
	struct sigaction  action;
  
	action.sa_handler =  &fpe_handler;
	sigemptyset (&action.sa_mask);
	/*See sigaction man page for why this is set*/
	action.sa_flags = SA_SIGINFO; 

	if (0 != (sigaction ( SIGFPE, &action, NULL)))
		fprintf (stderr, "Error:  setting up of FPE handler failed.\n");
	}
