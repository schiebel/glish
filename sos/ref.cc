#include <sos/ref.h>
#include <stdio.h>

sos_declare(PList,void);
typedef PList(void) void_list;

GcRef *current_cycle = 0;
ref_list *GcRef::cycle_roots = 0;

GcRef::GcRef( const GcRef &r ) : zero(0), unref(0), mask(0), ref_count(1)
	{
	if ( r.doPropagate( ) )
		SetUnref( r.unref );
	}

void GcRef::AddCycleRoot( GcRef *root )
	{
	if ( ! cycle_roots ) cycle_roots = new ref_list;
	cycle_roots->append( root );
	}

int GcRef::MirrorSet( ) const
	{
	return mMIRROR(mask);
	}

void GcRef::MarkMirror( )
	{
	mask |= mMIRROR();
	}

void GcRef::ClearMirror( )
	{
	mask &= ~mMIRROR();
	}

GcRef::cycle_type GcRef::CycleMode( ) const
	{
	return NONROOT;
	}

void GcRef::SetUnref( GcRef *r, int propagate_only )
	{
	if ( r )
		{
		if ( unref && (mUNREF(mask) || mPROPAGATE(mask) && unref != r) )
			{
			fprintf( stderr, "Uh Oh!!!\n" );
			ClearUnref( );
			}

		unref = r;
		mask |=  mPROPAGATE();

		if ( ! propagate_only )
			{
			mask |=  mUNREF();
			Ref( unref );
			}
		}
	}

void GcRef::ClearUnref( )
	{
	if ( unref )
		{
		if ( mUNREF(mask) ) Unref( unref );
		mask &= ~ (mUNREF() | mPROPAGATE());
		unref = 0;
		}
	}

void GcRef::AddZero( void *p )
	{
	if ( ! zero )
		{
		zero = p;
		mask &= ~mZEROLIST();
		mask |= mZERO();
		}
	else if ( mZEROLIST(mask) )
		{
		((void_list*)zero)->append(p);
		}
	else if ( mZERO(mask) )
		{
		void *t = zero;
		void_list *lst = new void_list;
		lst->append(t);
		lst->append(p);
		mask &= ~mZERO();
		mask |= mZEROLIST();
		}
	else
		{
		zero = p;
		mask &= ~mZEROLIST();
		mask |= mZERO();
		}
	}
		  
void GcRef::RemoveZero( void *p )
	{
	if ( ! p )
		{
		if ( ! mZERO(mask) && mZEROLIST(mask) )
			Unref( (void_list*) zero );
		zero = 0;
		mask &= ~mZERO();
		mask &= ~mZEROLIST();
		}
	else
		{
		if ( mZERO(mask) )
			{
			if ( zero == p )
				{
				zero = 0;
				mask &= ~mZERO();
				mask &= ~mZEROLIST();
				}
			}
		else if ( mZEROLIST(mask) )
			((void_list*)zero)->remove(p);
		}
	}

void GcRef::do_zero( )
	{
	if ( zero )
		{
		if ( mZERO(mask) )
			{
			*((void**)zero) = 0;
			}
		else if ( mZEROLIST(mask) )
			{
			void_list *lst = (void_list*) zero;
			for ( int i=0; i < lst->length(); ++i )
				if ((*lst)[i]) *((void**)(*lst)[i]) = 0;
			Unref( lst );
			}
		zero = 0;
		}
	}

void sos_do_unref( GcRef *ref )
	{
	Unref( ref );
	}
