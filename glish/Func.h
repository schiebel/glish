// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.
#ifndef func_h
#define func_h

#include "Expr.h"

class Sequencer;
class Stmt;
class stack_type;
class back_offsets_type;

typedef PList(IValue) args_list;

class Parameter;
glish_declare(PList,Parameter);
typedef PList(Parameter) parameter_list;

class observed_list : public ref_list {
    public:
	observed_list( observed_list &o ) : ref_list(o) {}
	observed_list(FINAL fh=0) : ref_list(fh) {}
	observed_list(int sz, FINAL fh=0) : ref_list(sz,fh) {}
	void prune(GcRef *a);
	void AppendList( observed_list * );
	int IsThisAnObservedList( );

	void ObservedGone( GcRef * );
	// observed_lists can be appeneded to other observed_lists...
	// this piggy-back approach turns out to be the only to allow
	// all of the depencencies to be attached to the necessary values
	void ObserverGone( GcRef *o );

	void RegisterOwner( IValue *v );
	void UnregisterOwner( IValue *v );

	void RegisterOwner( observed_list *v );
	void UnregisterOwner( observed_list *v );
	void UnrefGone( observed_list *u );

	~observed_list( );
    protected:
	ref_list pruned;
	ivalue_list owners;
	ref_list lowners;
};

class Observed : public GlishObject {
    public:
	Observed() { }
	Observed( const Observed &o ) : GlishObject(o) { }
	~Observed( );
	void watched_by( GcRef *o );
	void ObserverGone( GcRef *o );
	void ObserverChanged( GcRef *Old, GcRef *New );
    private:
	ref_list observers;
};

class Func : public Observed {
    public:
	virtual IValue* Call( evalOpt &opt, parameter_list* args ) = 0;

	int Mark() const	{ return mark; }
	void Mark( int m )	{ mark = m; }

    protected:
	int mark;
	};


class Parameter : public GlishObject {
    public:
	Parameter( const char* name, value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0,
			int is_empty = 0 );
	Parameter( char* name, value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0,
			int is_empty = 0 );
	Parameter( value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0,
			int is_empty = 0 );

	const char* Name() const		{ return name; }
	value_type ParamType() const		{ return parm_type; }
	Expr* Arg() const			{ return arg; }
	int IsEllipsis() const			{ return is_ellipsis; }
	int IsEmpty() const			{ return is_empty; }
	Expr* DefaultValue() const		{ return default_value; }

	// Number of values represented by "..." argument.
	int NumEllipsisVals( evalOpt & ) const;

	// Returns the nth value from a "..." argument; the first such
	// value is indexed using n=0.
	const IValue* NthEllipsisVal( evalOpt &, int n ) const;

	int Describe( OStream& s, const ioOpt &opt ) const;
	int Describe( OStream &s ) const
		{ return Describe( s, ioOpt() ); }

	~Parameter();

    protected:
	char* name;
	value_type parm_type;
	Expr* arg;
	int can_delete;
	int is_ellipsis;
	int is_empty;
	Expr* default_value;
	};


class FormalParameter : public Parameter {
    public:
	FormalParameter( const char* name, value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0 );
	FormalParameter( char* name, value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0 );
	FormalParameter( value_type parm_type, Expr* arg,
			int is_ellipsis = 0, Expr* default_value = 0 );

	int Describe( OStream& s, const ioOpt &opt ) const;
	int Describe( OStream &s ) const
		{ return Describe( s, ioOpt() ); }
	};

class ActualParameter : public Parameter {
    public:
	ActualParameter( const char* name_, value_type parm_type_, Expr* arg_,
			int is_ellipsis_ = 0, Expr* default_value_ = 0,
			int is_empty_ = 0 )
		: Parameter( name_, parm_type_, arg_, is_ellipsis_,
				default_value_, is_empty_ )
		{
		}
	ActualParameter( char* name_, value_type parm_type_, Expr* arg_,
			int is_ellipsis_ = 0, Expr* default_value_ = 0,
			int is_empty_ = 0 )
		: Parameter( name_, parm_type_, arg_, is_ellipsis_,
				default_value_, is_empty_ )
		{
		}
	ActualParameter( value_type parm_type_, Expr* arg_,
			int is_ellipsis_ = 0, Expr* default_value_ = 0,
			int is_empty_ = 0 )
		: Parameter( parm_type_, arg_, is_ellipsis_,
				default_value_, is_empty_ )
		{
		}

	// A missing parameter.
	ActualParameter() : Parameter( VAL_VAL, 0, 0, 0, 1 )
		{
		}
	};

class UserFuncKernel : public GlishObject {
    public:
	UserFuncKernel( parameter_list* formals, Stmt* body, int size,
			back_offsets_type *back_refs, Sequencer* sequencer,
			Expr* subsequence_expr, const IValue *attributes, IValue *&err );

	~UserFuncKernel();

	IValue* Call( evalOpt &opt, parameter_list* args, stack_type *stack = 0);
	IValue* DoCall( evalOpt &opt, stack_type *stack = 0 );

	int Describe( OStream& s, const ioOpt &opt ) const;
	int Describe( OStream &s ) const
		{ return Describe( s, ioOpt() ); }

    protected:
	IValue* EvalParam( evalOpt &, Parameter* p, Expr* actual, IValue *&fail );

	// Decode an actual "..." argument.
	// returning 0 means OK, non-zero indicates error
	IValue *AddEllipsisArgs( evalOpt &, Frame *, int &arg_cnt, Parameter *actual_ellipsis,
				int& num_args, int num_formals,
				IValue* formal_ellipsis_value );

	// Add to a formal "..." parameter.
	void AddEllipsisValue( evalOpt &, IValue *ellipsis_value, Expr *arg );

	// returning 0 means OK, non-zero indicates error
	IValue *ArgOverFlow( evalOpt &, Expr* arg, int num_args, int num_formals,
				IValue* ellipsis_value );

	parameter_list* formals;
	back_offsets_type *back_refs;
	Stmt* body;
	int frame_size;
	Sequencer* sequencer;
	Expr* subsequence_expr;
	int reflect_events;
	int valid;
	int has_ellipsis;
	int ellipsis_position;
	};

class UserFunc : public Func {
    public:
	UserFunc( parameter_list* formals, Stmt* body, int size,
		  back_offsets_type *back_refs, Sequencer* sequencer,
		  Expr* subsequence_expr, IValue *&err,
		  const IValue *attributes=0, ivalue_list *misc_values = 0 );
	UserFunc( const UserFunc *f );

	~UserFunc();

	IValue* Call( evalOpt &opt, parameter_list* args );

	void EstablishScope();
	UserFunc *clone() { return new UserFunc(this); }

	int Describe( OStream& s, const ioOpt &opt ) const;
	int Describe( OStream &s ) const
		{ return Describe( s, ioOpt() ); }

	// value can be part of a frame so it can't be deleted, but needs to have
	// everything possible freed...
	int SoftDelete( );

    protected:
	Sequencer* sequencer;
	UserFuncKernel *kernel;
	int scope_established;
	stack_type *stack;
	ivalue_list *misc;	// extra values to be protected
				// from garbage collection
	};

glish_declare(PList,UserFunc);
typedef PList(UserFunc) func_list;

extern void describe_parameter_list( parameter_list* params, OStream& s );
extern void copy_funcs( void *to_, void *from_, size_t len );

#endif /* func_h */
