// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef expr_h
#define expr_h

#include "Glish/Dict.h"
#include "IValue.h"
#include "Frame.h"

class Stmt;
class Expr;
class Func;
class UserFunc;
class EventDesignator;
class Sequencer;
class ParameterPList;
class Frame;

declare(PList,Expr);
declare(PDict,Expr);

typedef PList(Expr) expr_list;

extern int shutting_glish_down;

class ParseNode : public GlishObject {
    public:
	ParseNode() { }
	ParseNode( const char *desc ) : GlishObject(desc) { }
	virtual int canDelete() const;
	};

inline void NodeRef( GlishObject *obj )
	{
	Ref( obj );
	}
inline void NodeUnref( GlishObject *obj )
	{
	Unref( obj );
	}
inline void NodeUnref( ParseNode *obj )
	{
	if ( obj && obj->canDelete() )
		Unref( obj );
	}

// Different variable access; used by the VarExpr
//	PARSE_ACCESS --  the variable has been parsed for the purpose of
//			 constructing a frame
//      USE_ACCESS   --  the variable has been used in an expression
//
typedef enum { PARSE_ACCESS, USE_ACCESS } access_type;

// Various ways the scope of a value can be modified
//      SCOPE_UNKNOWN --  no particular modification
//      SCOPE_LHS     --  left hand side of an assignment
//      SCOPE_RHS     --  right hand side of an assignment
//
typedef enum { SCOPE_UNKNOWN, SCOPE_LHS, SCOPE_RHS } scope_modifier;

// Different types of expression evaluation: evaluate and return a
// modifiable copy of the result; evaluate and return a read-only
// version of the result (which will subsequently be released using
// Expr::ReadOnlyDone); or evaluate for side effects only, and return
// nothing.
typedef enum { EVAL_COPY, EVAL_READ_ONLY, EVAL_SIDE_EFFECTS } eval_type;


typedef void (*change_var_notice)(IValue*,IValue*);
class Expr : public ParseNode {
    public:
	Expr( const char* desc ) : ParseNode( desc ) { }

	// Returns a copy of the present value of the event expression.
	// The caller is responsible for deleting the copy when done
	// using it.
	IValue* CopyEval()
		{ return Eval( EVAL_COPY ); }

	// Returns a read-only copy (i.e., the original) of the present
	// value of the event expression.  The caller is responsible for
	// later calling ReadOnlyDone() when the copy is no longer needed.
	const IValue* ReadOnlyEval()
		{ return Eval( EVAL_READ_ONLY ); }

	// Declares that the previously returned ReadOnlyEval() value
	// is no longer needed.
	void ReadOnlyDone( const IValue* returned_value )
		{ Unref( (IValue*) returned_value ); }


	// Returns the present value of the event expression.  If
	// "modifiable" is true then a modifiable version of the value is
	// returned; otherwise, a read-only copy.
	virtual IValue* Eval( eval_type etype ) = 0;

	// Returns true if this expression is going to do an echo of itself
	// AS PART OF EVALUATION, I.E. "Eval()", if trace is turned on.
	virtual int DoesTrace( ) const;

	// Evaluates the Expr just for side-effects.
	virtual IValue *SideEffectsEval();


	// Returns a reference to the value of the event expression.
	// If val_type is VAL_REF then a "ref" reference is returned,
	// otherwise a "const" reference.
	//
	// The reference should be Unref()'d once done using it.
	virtual IValue* RefEval( value_type val_type );


	// Assigns a new value to the variable (LHS) corresponding
	// to this event expression, if appropriate.  The passed
	// value becomes the property of the Expr, which must
	// subsequently take care of garbage collecting it as necessary
	// (in particular, next time a value is Assign()'d, the value
	// should be deleted).
	//
	// Note that new_value can be nil (providing that index is nil,
	// too), in which case the old value
	// is deleted and the value set to nil.  Used for things like
	// formal parameters where it's desirable to free up the memory
	// used by their values as soon as the function call is complete,
	// rather than waiting for the next call to the function (and
	// subsequent assignment to the formal parameters).
	virtual IValue *Assign( IValue* new_value );

	// Returns true if, when evaluated as a statement, this expression's
	// value should be "invisible" - i.e., the statement's value is "no
	// value" (false).
	virtual int Invisible() const;

	// Because the scoping of a value can be determined by its location
	// within an expression, e.g. LHS or RHS, this function is a wrapper
	// around the function which actually does the work, i.e.
	// DoBuildFrameInfo(). DoBuildFrameInfo() traverses the tree and
	// fixes up any unresolved variables. This wrapper is required
	// so that any "Expr*"s which removed from the tree and placed on
	// the deletion list can be cleaned up. "VarExpr"s can be pruned
	// from the tree as their scope is resolved.
	Expr *BuildFrameInfo( scope_modifier );

	// This should not be called outside of the Expr hierarchy. Use
	// BuildFrameInfo() instead.
	virtual Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	virtual ~Expr();

	// Where it is important, these functions allow a notify function
	// to be set which will be called when the Expr is modified.
	virtual void SetChangeNotice(change_var_notice);
	virtual void ClearChangeNotice( );

    protected:
	// Return either a copy of the given value, or a reference to
	// it, depending on etype.  If etype is EVAL_SIDE_EFFECTS, a
	// warning is generated and 0 returned.
	IValue* CopyOrRefValue( const IValue* value, eval_type etype );
	};


class VarExpr : public Expr {
    public:
	VarExpr( char* var_id, scope_type scope, int scope_offset,
			int frame_offset, Sequencer* sequencer );

	VarExpr( char *var_id, Sequencer *sequencer );

	~VarExpr();

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );

	const char* VarID()	{ return id; }
	int offset()		{ return frame_offset; }
	int soffset()		{ return scope_offset; }
	scope_type Scope()	{ return scope; }
	change_var_notice change_func() const { return func; }
	void set( scope_type scope, int scope_offset, int frame_offset );

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	access_type Access() { return access; }

	// Used by Sequencer::DeleteVal (and in turn by 'symbol_delete()')
	void change_id( char * );

	void SetChangeNotice(change_var_notice);
	void ClearChangeNotice( );

	//
	// These prevent/allow these preliminary frames to be used
	// in preference to 'sequencer'. It is necessary to disable
	// these preliminary frames when "invoking" the function
	// (i.e. just after the parameters have been assigned) due
	// to recursive function calls.
	//
	void HoldFrames( ) { hold_frames = 1; }
	void ReleaseFrames( ) { hold_frames = 0; }

	//
	// These are local frames which are only used in the process
	// of establishing invocation parameters for a function.
	// 'PopFrame()' does an implicit 'HoldFrames()' because this
	// is typically what you want when filling function parameters,
	// and 'PushFrame()' does an implicit 'ReleaseFrames()'.
	//
	// These 'Push/PopFrame()', 'Hold/ReleaseFrame()' functions
	// were needed for function invocation to handle things like:
	//
	//	func foo( x, y = x * 8 ) { return y }
	//
	//
	void PushFrame( Frame *f ) { frames.append(f); ReleaseFrames(); }
	void PopFrame( );

	IValue *Assign( IValue* new_value );
	// This Assignment forces VarExpr to use 'f' instead of going
	// to the 'sequencer'. This result in a 'PushFrame(f)' too.
	IValue *Assign( IValue* new_value, Frame *f )
		{ PushFrame( f ); Assign( new_value ); }

    protected:
	char* id;
	scope_type scope;
	int frame_offset;
	int scope_offset;
	Sequencer* sequencer;
	access_type access;
	change_var_notice func;
	frame_list frames;
	int hold_frames;
	};


class ScriptVarExpr : public VarExpr {
    public:
	ScriptVarExpr( char* vid, scope_type sc, int soff, 
			int foff, Sequencer* sq ) 
		: VarExpr( vid, sc, soff, foff, sq ) { }

	ScriptVarExpr( char *vid, Sequencer *sq )
		: VarExpr ( vid, sq ) { }

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	~ScriptVarExpr();
	};


// These functions are used to create the VarExpr. There may be times
// when a  variable must be initialized just before it is first used,
// e.g. "script" which cannot be initialized earlier because it isn't
// known if it is multithreaded or not.
VarExpr *CreateVarExpr( char *id, Sequencer *seq );
VarExpr *CreateVarExpr( char *id, scope_type scope, int scope_offset,
			int frame_offset, Sequencer *seq,
			change_var_notice f=0 );

class ValExpr : public Expr {
    public:
	ValExpr( IValue *v ) : Expr("value"), val(v) { Ref(val); }

	~ValExpr();

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );

    protected:
	IValue *val;
	};

class ConstExpr : public Expr {
    public:
	ConstExpr( const IValue* const_value );

	IValue* Eval( eval_type etype );
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~ConstExpr();

    protected:
	const IValue* const_value;
	};


class FuncExpr : public Expr {
    public:
	FuncExpr( UserFunc* f );

	IValue* Eval( eval_type etype );
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~FuncExpr();

    protected:
	UserFunc* func;
	};


class UnaryExpr : public Expr {
    public:
	UnaryExpr( Expr* operand, const char* desc );

	IValue* Eval( eval_type etype ) = 0;
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	~UnaryExpr();

    protected:
	Expr* op;
	};


class BinaryExpr : public Expr {
    public:
	BinaryExpr( Expr* op1, Expr* op2, const char* desc );

	IValue* Eval( eval_type etype ) = 0;
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	~BinaryExpr();

    protected:
	Expr* left;
	Expr* right;
	};



class NegExpr : public UnaryExpr {
    public:
	NegExpr( Expr* operand );

	IValue* Eval( eval_type etype );
	};


class NotExpr : public UnaryExpr {
    public:
	NotExpr( Expr* operand );

	IValue* Eval( eval_type etype );
	};


class AssignExpr : public BinaryExpr {
    public:
	AssignExpr( Expr* op1, Expr* op2 );

	IValue* Eval( eval_type etype );
	IValue *SideEffectsEval();
	int Invisible() const;

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );
	};


class OrExpr : public BinaryExpr {
    public:
	OrExpr( Expr* op1, Expr* op2 );

	IValue* Eval( eval_type etype );
	};


class AndExpr : public BinaryExpr {
    public:
	AndExpr( Expr* op1, Expr* op2 );

	IValue* Eval( eval_type etype );
	};


class ConstructExpr : public Expr {
    public:
	ConstructExpr( ParameterPList* args );

	IValue* Eval( eval_type etype );
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~ConstructExpr();

    protected:
	IValue* BuildArray();
	IValue* BuildRecord();

	//
	//  0 => OK, !0 == error value
	//
	IValue *AllEquivalent( const IValue* values[], int num_values,
				glish_type& max_type );
	IValue *TypeCheck( const IValue* values[], int num_values,
			       glish_type& max_type );
	IValue *MaxNumeric( const IValue* values[], int num_values,
				glish_type& max_type );

	IValue* ConstructArray( const IValue* values[], int num_values,
				int total_length, glish_type max_type );

	int is_array_constructor;
	ParameterPList* args;
	const char *err;
	};


class ArrayRefExpr : public UnaryExpr {
    public:
	ArrayRefExpr( Expr* op1, expr_list* a );

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );

	IValue *Assign( IValue* new_value );

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~ArrayRefExpr();

    protected:
	IValue *CallFunc(Func *fv, eval_type etype, ParameterPList *);
	expr_list* args;
	};


class RecordRefExpr : public UnaryExpr {
    public:
	RecordRefExpr( Expr* op, char* record_field );

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );

	IValue *Assign( IValue* new_value );

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~RecordRefExpr();

    protected:
	char* field;
	};


class AttributeRefExpr : public BinaryExpr {
    public:
	AttributeRefExpr( Expr* op1 );
	AttributeRefExpr( Expr* op1, Expr* op2 );
	AttributeRefExpr( Expr* op, char* attribute );

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );

	IValue *Assign( IValue* new_value );

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	Expr *DoBuildFrameInfo( scope_modifier, expr_list & );

	~AttributeRefExpr();

    protected:
	char* field;
	};


class RefExpr : public UnaryExpr {
    public:
	RefExpr( Expr* op, value_type type );

	IValue* Eval( eval_type etype );
	IValue *Assign( IValue* new_value );

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

    protected:
	value_type type;
	};


class RangeExpr : public BinaryExpr {
    public:
	RangeExpr( Expr* op1, Expr* op2 );

	IValue* Eval( eval_type etype );
	};


class CallExpr : public UnaryExpr {
    public:
	CallExpr( Expr* func, ParameterPList* args, Sequencer *seq_arg );

	IValue* Eval( eval_type etype );
	IValue *SideEffectsEval();
	int DoesTrace( ) const;

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~CallExpr();

    protected:
	ParameterPList* args;
	Sequencer* sequencer;
	};

class IncludeExpr : public UnaryExpr {
    public:
	IncludeExpr( Expr* file, Sequencer *seq_arg );
	IValue* Eval( eval_type etype );
    protected:
	Sequencer* sequencer;
	};


class SendEventExpr : public Expr {
    public:
	SendEventExpr( EventDesignator* sender, ParameterPList* args,
			int is_request_reply );

	IValue* Eval( eval_type etype );
	IValue* SideEffectsEval();

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	~SendEventExpr();

    protected:
	EventDesignator* sender;
	ParameterPList* args;
	int is_request_reply;
	};


typedef enum { EVENT_AGENT, EVENT_NAME, EVENT_VALUE } last_event_type;

class LastEventExpr : public Expr {
    public:
	LastEventExpr( Sequencer* sequencer, last_event_type type );

	IValue* Eval( eval_type etype );
	IValue* RefEval( value_type val_type );
	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

    protected:
	Sequencer* sequencer;
	last_event_type type;
	};


extern void describe_expr_list( const expr_list* list, OStream& s );


#endif /* expr_h */
