#include <sos/ref.h>
#include <stdio.h>

sos_declare(PList,void);
typedef PList(void) void_list;

GcRef *current_cycle = 0;
ref_list *GcRef::cycle_roots = 0;

GcRef::GcRef( const GcRef &r ) : unref(0), mask(0), ref_count(1)
	{
	if ( r.doPropagate( ) )
		SetUnref( r.unref );
	}

void GcRef::AddCycleRoot( GcRef *root )
	{
	if ( ! cycle_roots ) cycle_roots = new ref_list;
	cycle_roots->append( root );
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

		if ( ! propagate_only && ! mUNREF(mask) )
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

void GcRef::unref_revert( )
	{
	if ( unref && doPropagate() && ! doUnref() )
		{
// 		Ref(unref);
		if ( ref_count-1 > 0xFFFF >> 4 )
			{
			// Here we just can't revert to the no-unref status
			// because the reference count we need to save is
			// greater than the bits we have available to store it
			// So we must leak the memory.
			mask |= mUNREF();
			}
		else
			{
			// We store off the current reference count, and when it
			// it reaches zero, we revert to propagate with no unref
			// and the old reference count.
			set_revert_count( ref_count-1 );
			ref_count = 1;
			mask |= mUNREF_REVERT();
			}
		}
	}

int GcRef::SoftDelete( )
	{
	fprintf( stderr, "\nSOFT DELETE REQUIRED\n" );
	return 0;
	}

GcRef::~GcRef() { }

void sos_do_unref( GcRef *object )
	{
	if ( object->doRevert( ) )
		{
		object->mask &= ~ object->mUNREF_REVERT();
		object->ref_count = object->get_revert_count( );
		}
	else
		{
		if ( object->doUnref() ) Unref( object->unref );

		if ( object->FrameMember( ) )
			{
			if ( object->SoftDelete( ) ) delete object;
			}
		else
			delete object;

		}
	}
