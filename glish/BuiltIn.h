// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef built_in_h
#define built_in_h

#include "Glish/List.h"
#include "IValue.h"
#include "Func.h"


typedef List(const_ivalue) const_args_list;

struct dcomplex;


#define NUM_ARGS_VARIES -1

class BuiltIn : public Func {
    public:
	BuiltIn( const char* name, int num, int do_ref_eval_=0 )
		{
		description = name;
		num_args = num;
		do_deref = 1;
		side_effects_call_okay = 0;
		handle_fail = 0;
		do_ref_eval = do_ref_eval_;
		}

	const char* Name()				{ return description; }
	IValue* Call( parameter_list* args, eval_type etype );

	virtual IValue* DoCall( const_args_list* args_vals ) = 0;

	// Used when the call is just for side-effects; sets side_effects_okay
	// to true if it was okay to call this function just for side effects.
	virtual void DoSideEffectsCall( const_args_list* args_vals,
					int& side_effects_okay );

	// If do_deref is true then all reference arguments are dereferenced.
	// If false, they are left alone.
	void SetDeref( int deref )
		{ do_deref = deref; }
	void SetFailHandling( int do_fail )
		{ handle_fail = do_fail; }

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

    protected:
	// returns 0 if everything is OK
	IValue *AllNumeric( const_args_list* args_vals, glish_type& max_type,
			int strings_okay = 0 );

	int num_args;
	int do_deref;
	// true if side-effects-only call is okay
	int side_effects_call_okay;
	int handle_fail;
	int do_ref_eval;
	};


typedef IValue* (*value_func_1_value_arg)( const IValue* );

class OneValueArgBuiltIn : public BuiltIn {
    public:
	OneValueArgBuiltIn( value_func_1_value_arg arg_func,
				const char* name ) : BuiltIn(name, 1)
		{ func = arg_func; }

	IValue* DoCall( const_args_list* args_val );

    protected:
	value_func_1_value_arg func;
	};


typedef double (*double_func_1_double_arg)( double );
typedef dcomplex (*dcomplex_func_1_dcomplex_arg)( const dcomplex );

class NumericVectorBuiltIn : public BuiltIn {
    public:
	NumericVectorBuiltIn( double_func_1_double_arg arg_func,
				dcomplex_func_1_dcomplex_arg arg_cfunc,
				const char* name ) : BuiltIn(name, 1)
		{ func = arg_func; cfunc = arg_cfunc; }

	IValue* DoCall( const_args_list* args_val );

    protected:
	double_func_1_double_arg func;
	dcomplex_func_1_dcomplex_arg cfunc;
	};


#define DERIVE_BUILTIN(name,num_args,description,init)			\
class name : public BuiltIn {						\
    public:								\
	name() : BuiltIn(description, num_args)	{ init }		\
	IValue* DoCall( const_args_list* args_vals );			\
	};


DERIVE_BUILTIN(RealBuiltIn,1,"real",)
DERIVE_BUILTIN(ImagBuiltIn,1,"imag",)
DERIVE_BUILTIN(StrlenBuiltIn,1,"strlen",)
DERIVE_BUILTIN(ComplexBuiltIn,NUM_ARGS_VARIES,"complex",)
DERIVE_BUILTIN(SumBuiltIn,NUM_ARGS_VARIES,"sum",)
DERIVE_BUILTIN(ProdBuiltIn,NUM_ARGS_VARIES,"prod",)
DERIVE_BUILTIN(LengthBuiltIn,NUM_ARGS_VARIES,"length",)
DERIVE_BUILTIN(RangeBuiltIn,NUM_ARGS_VARIES,"range",)
DERIVE_BUILTIN(SeqBuiltIn,NUM_ARGS_VARIES,"seq",)
DERIVE_BUILTIN(RepBuiltIn,2,"rep",)
DERIVE_BUILTIN(NumArgsBuiltIn,NUM_ARGS_VARIES,"num_args",)
DERIVE_BUILTIN(NthArgBuiltIn,NUM_ARGS_VARIES,"nth_arg",)
DERIVE_BUILTIN(RandomBuiltIn,NUM_ARGS_VARIES,"random",)
DERIVE_BUILTIN(CbindBuiltIn,NUM_ARGS_VARIES,"cbind",)
DERIVE_BUILTIN(RbindBuiltIn,NUM_ARGS_VARIES,"rbind",)

DERIVE_BUILTIN(IsModifiableBuiltIn,1,"is_modifiable",)

DERIVE_BUILTIN(PasteBuiltIn,NUM_ARGS_VARIES,"internal_paste",)
DERIVE_BUILTIN(SplitBuiltIn,NUM_ARGS_VARIES,"split",)
DERIVE_BUILTIN(SizeofBuiltIn,1,"sizeof",)

DERIVE_BUILTIN(IsNaNBuiltIn,1,"is_nan",)

DERIVE_BUILTIN(ReadValueBuiltIn,1,"read_value",)
DERIVE_BUILTIN(WriteValueBuiltIn,2,"write_value",side_effects_call_okay = 1;)

DERIVE_BUILTIN(WheneverStmtsBuiltIn,1,"whenever_stmts",)

DERIVE_BUILTIN(ActiveAgentsBuiltIn,0,"active_agents",)
DERIVE_BUILTIN(TimeBuiltIn,0,"time",)

class IsConstBuiltIn : public BuiltIn {
    public:
	IsConstBuiltIn() : BuiltIn("is_const", 1, 1)	{  }
	IValue* DoCall( const_args_list* args_vals );
	};

#define DERIVE_SEQUENCER_BUILTIN(name,num_args,description)		\
class name : public BuiltIn {						\
    public:								\
	name( Sequencer* arg_sequencer )				\
	    : BuiltIn(description, num_args)				\
		{ sequencer = arg_sequencer; }				\
	IValue* DoCall( const_args_list* args_vals );			\
									\
    protected:								\
	Sequencer* sequencer;						\
	};


DERIVE_SEQUENCER_BUILTIN(CreateAgentBuiltIn,0,"create_agent")
DERIVE_SEQUENCER_BUILTIN(MissingBuiltIn,0,"missing")

#ifdef GGC
DERIVE_SEQUENCER_BUILTIN(GarbageBuiltIn,0,"collect_garbage")
#endif

DERIVE_SEQUENCER_BUILTIN(SymbolNamesBuiltIn,NUM_ARGS_VARIES,"symbol_names")
DERIVE_SEQUENCER_BUILTIN(SymbolValueBuiltIn,1,"symbol_value")
DERIVE_SEQUENCER_BUILTIN(SymbolSetBuiltIn,NUM_ARGS_VARIES,"symbol_set")
DERIVE_SEQUENCER_BUILTIN(SymbolDeleteBuiltIn,1,"symbol_delete")

DERIVE_SEQUENCER_BUILTIN(WheneverActiveBuiltIn,1,"whenever_active")
DERIVE_SEQUENCER_BUILTIN(LastWheneverExecutedBuiltIn,0,"last_whenever_executed")
DERIVE_SEQUENCER_BUILTIN(CurrentWheneverBuiltIn,0,"current_whenever")
DERIVE_SEQUENCER_BUILTIN(EvalBuiltIn,NUM_ARGS_VARIES,"eval")

//
// These will move to a client at some point.
// 
DERIVE_SEQUENCER_BUILTIN(CreateGraphicBuiltIn,NUM_ARGS_VARIES,"create_graphic")
DERIVE_BUILTIN(TkBuiltIns,1,"tk_builtin_funcs",)


extern char* paste( parameter_list* args );
extern char* paste( const_args_list* args );

extern void create_built_ins( Sequencer* s, const char *program_name );

#endif /* built_in_h */
