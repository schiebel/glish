// $Header$

#ifndef func_h
#define func_h

#include "Expr.h"

class Sequencer;
class Stmt;
class PList(Frame);

typedef IValue* value_ptr;
#define value_ptr_to_void void_ptr
#define void_to_value_ptr value_ptr
declare(List,value_ptr);
typedef List(value_ptr) args_list;

class Parameter;
declare(PList,Parameter);
typedef PList(Parameter) parameter_list;


class Func : public GlishObject {
    public:
	virtual IValue* Call( parameter_list* args, eval_type etype ) = 0;

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
	int NumEllipsisVals() const;

	// Returns the nth value from a "..." argument; the first such
	// value is indexed using n=0.
	const IValue* NthEllipsisVal( int n ) const;

	void Describe( ostream& s ) const;

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

	void Describe( ostream& s ) const;
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
			Sequencer* sequencer, Expr* subsequence_expr, IValue *&err );
	~UserFuncKernel();

	IValue* Call( parameter_list* args, eval_type etype, PList(Frame)* local_frames = 0);
	IValue* DoCall( args_list* args_vals, eval_type etype, IValue* missing, PList(Frame)* local_frames = 0 );

	void Describe( ostream& s ) const;

    protected:
	IValue* EvalParam( Parameter* p, Expr* actual, IValue *&fail );

	// Decode an actual "..." argument.
	// returning 0 means OK, non-zero indicates error
	IValue *AddEllipsisArgs( args_list* args_vals, Parameter* actual_ellipsis,
				int& num_args, int num_formals,
				IValue* formal_ellipsis_value );

	// Add to a formal "..." parameter.
	void AddEllipsisValue( IValue* ellipsis_value, Expr* arg );

	// returning 0 means OK, non-zero indicates error
	IValue *ArgOverFlow( Expr* arg, int num_args, int num_formals,
				IValue* ellipsis_value );

	parameter_list* formals;
	Stmt* body;
	int frame_size;
	Sequencer* sequencer;
	Expr* subsequence_expr;
	int valid;
	int has_ellipsis;
	int ellipsis_position;
	};

class UserFunc : public Func {
    public:
	UserFunc( parameter_list* formals, Stmt* body, int size,
			Sequencer* sequencer, Expr* subsequence_expr, IValue *&err );
	UserFunc( const UserFunc *f );

	~UserFunc();

	IValue* Call( parameter_list* args, eval_type etype );

	void EstablishScope();
	UserFunc *clone() { return new UserFunc(this); }

	void Describe( ostream& s ) const;

    protected:
	Sequencer* sequencer;
	UserFuncKernel *kernel;
	int scope_established;
	PList(Frame)* local_frames;
	};


extern void describe_parameter_list( parameter_list* params, ostream& s );

#endif /* func_h */
