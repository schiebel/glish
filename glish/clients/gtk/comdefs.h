#ifndef comdefs_h_
#define comdefs_h_

#define SP " "

#define HANDLE_CTOR_ERROR(STR)					\
		{						\
		frame = 0;					\
		SetError( (Value*) generate_error( STR ) );	\
		return;						\
		}

#define InvalidArg( num )						\
	{								\
	global_store->Error( "invalid type for argument " ## #num );	\
	return;								\
	}

#define InvalidNumberOfArgs( num )					\
	{								\
	global_store->Error( "invalid number of arguments, " ## #num ## " expected" );\
	return;								\
	}

#define SETINIT								\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value");			\
		return;							\
		}							\
									\
	Ref( args );							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

#define SETDONE Unref(args);

#define SETVAL(var,condition)						\
	const Value *var      = rptr->NthEntry( c++, key );		\
	if ( ! ( condition) )						\
		InvalidArg(c-1);

#define SETSTR(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0 )		\
	charptr var = ( var##_v_ ->StringPtr(0) )[0];
#define SETDIM(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0   ||		\
				var##_v_ ->IsNumeric() )		\
	char var##_char_[30];						\
	charptr var = 0;						\
	if ( var##_v_ ->Type() == TYPE_STRING )				\
		var = ( var##_v_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		sprintf(var##_char_,"%d", var##_v_ ->IntVal());	\
		var = var##_char_;					\
		}
#define SETINT(var)							\
	SETVAL(var##_v_, var##_v_ ->IsNumeric() &&			\
				var##_v_ ->Length() > 0 )		\
	int var = var##_v_ ->IntVal();

#define SETDOUBLE(var)							\
	SETVAL(var##_v_, var##_v_ ->IsNumeric() &&			\
				var##_v_ ->Length() > 0 )		\
	double var = var##_v_ ->DoubleVal();

#define EXPRINIT(EVENT)							\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
									\
	/*Ref(args);*/							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

#define EXPRVAL(var,EVENT)						\
	const Value *var = rptr->NthEntry( c++, key );			\
	if ( ! var )							\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}

#define EXPRSTRVALXX(var,EVENT,LINE)					\
	const Value *var = rptr->NthEntry( c++, key );			\
	LINE								\
	if ( ! var || var ->Type() != TYPE_STRING ||			\
		var->Length() <= 0 )					\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}

#define EXPRSTRVAL(var,EVENT) EXPRSTRVALXX(var,EVENT,)

#define EXPRSTR(var,EVENT)						\
	charptr var = 0;						\
	EXPRSTRVALXX(var##_val_, EVENT,)				\
	var = ( var##_val_ ->StringPtr(0) )[0];

#define EXPRDIM(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
	charptr var = 0;						\
	char var##_char_[30];						\
	if ( ! var##_val_ || ( var##_val_ ->Type() != TYPE_STRING &&	\
			       ! var##_val_ ->IsNumeric() ) ||		\
		var##_val_ ->Length() <= 0 )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	else								\
		if ( var##_val_ ->Type() == TYPE_STRING	)		\
			var = ( var##_val_ ->StringPtr(0) )[0];		\
		else							\
			{						\
			sprintf(var##_char_,"%d", var##_val_->IntVal());\
			var = var##_char_;				\
			}

#define EXPRINTVALXX(var,EVENT,LINE)                                    \
        const Value *var = rptr->NthEntry( c++, key );			\
        LINE                                                            \
        if ( ! var || ! var ->IsNumeric() || var ->Length() <= 0 )      \
                {                                                       \
                global_store->Error("bad value: %s", EVENT);            \
                return 0;                                   		\
                }

#define EXPRINTVAL(var,EVENT)  EXPRINTVALXX(var,EVENT, const Value *var##_val_ = var;)

#define EXPRINT(var,EVENT)                                              \
        int var = 0;                                                    \
	EXPRINTVALXX(var##_val_,EVENT,)                                 \
        var = var##_val_ ->IntVal();

#define EXPRINT2(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
        char var##_char_[30];						\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Length() <= 0 )		\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	if ( var##_val_ -> IsNumeric() )				\
		{							\
		int var##_int_ = var##_val_ ->IntVal();			\
		var = var##_char_;					\
		sprintf(var##_char_,"%d",var##_int_);			\
		}							\
	else if ( var##_val_ -> Type() == TYPE_STRING )			\
		var = ( var##_val_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		global_store->Error("bad type: %s", EVENT);		\
		return 0;						\
		}

#define EXPRDOUBLE(var,EVENT)                                           \
        const Value *var##_val_ = rptr->NthEntry( c++, key );           \
        double var = 0;                                                 \
        if ( ! var##_val_ || ! var##_val_ ->IsNumeric() ||              \
                var##_val_ ->Length() <= 0 )                            \
                {                                                       \
                global_store->Error("bad value: %s", EVENT);            \
                return 0;                                   		\
                }                                                       \
        else                                                            \
                var = var##_val_ ->DoubleVal();

#define EXPRDOUBLE2(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
        char var##_char_[30];						\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Length() <= 0 )		\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	if ( var##_val_ -> IsNumeric() )				\
		{							\
		double var##_double_ = var##_val_ ->DoubleVal();	\
		var = var##_char_;					\
		sprintf(var##_char_,"%f",var##_double_);		\
		}							\
	else if ( var##_val_ -> Type() == TYPE_STRING )			\
		var = ( var##_val_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		global_store->Error("bad type: %s", EVENT);		\
		return 0;						\
		}

#define EXPRDOUBLEPTR(var, NUM, EVENT)					\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
	double *var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Type() != TYPE_DOUBLE ||	\
		var##_val_ ->Length() < NUM )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	else								\
		var = var##_val_ ->DoublePtr(0);

#define EXPR_DONE(var)

#define HASARG( args, cond )						\
	if ( ! (args->Length() cond) )					\
		{							\
		global_store->Error("wrong number of arguments");	\
		return 0;						\
		}

#define DEFINE_DTOR(CLASS)				\
CLASS::~CLASS( )					\
	{						\
	if ( frame )					\
		{					\
		frame->RemoveElement( this );		\
		frame->Pack();				\
		}					\
	UnMap();					\
	}

#define CREATE_RETURN						\
	if ( ! ret || ! ret->IsValid() )			\
		{						\
		Value *err = ret->GetError();			\
		if ( err )					\
			{					\
			global_store->Error( err );		\
			Unref( err );				\
			}					\
		else						\
			global_store->Error( "tk widget creation failed" ); \
		}						\
	else							\
		ret->SendCtor("newtk");				\
								\
	SETDONE

#endif
