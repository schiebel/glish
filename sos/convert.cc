#include "convert.h"

struct swap_ab_struct {
	char one;
	char two;
};

struct swap_abcd_struct {
	char one;
	char two;
	char three;
	char four;
};

struct swap_abcdefgh_struct {
	char  one;
	char  two;
	char  three;
	char  four;
	char  five;
	char  six;
	char  seven;
	char  eight;
};

void swap_ab_ba(char *buffer, unsigned int len)
	{
	register struct swap_ab_struct *ptr = (swap_ab_struct *) buffer;
	for (register unsigned int i = 0; i < len; i++, ptr++)
		{
		register char tmp = ptr->one;
		ptr->one = ptr->two;
		ptr->two = tmp;
		}
	}

void swap_abcd_dcba(char *buffer, unsigned int len)
	{
	register struct swap_abcd_struct *ptr = (swap_abcd_struct *) buffer;
	for (register unsigned int i = 0; i < len; i++, ptr++)
		{
		register char tmp = ptr->one;
		ptr->one = ptr->four;
		ptr->four = tmp;
		tmp = ptr->two;
		ptr->two = ptr->three;
		ptr->three = tmp;
		}
	}

void swap_abcdefgh_hgfedcba(char *buffer, unsigned int len)
	{
	register struct swap_abcdefgh_struct *ptr = (swap_abcdefgh_struct *) buffer;
	for (register unsigned int i = 0; i < len; i++, ptr++)
		{
		register char tmp = ptr->one;
		ptr->one = ptr->eight;
		ptr->eight = tmp;
		tmp = ptr->two;
		ptr->two = ptr->seven;
		ptr->seven = tmp;
		tmp = ptr->three;
		ptr->three = ptr->six;
		ptr->six = tmp;
		tmp = ptr->four;
		ptr->four = ptr->five;
		ptr->five = tmp;
		}
	}


#define VAX_SNG_BIAS  0x81
#define IEEE_SNG_BIAS  0x7f
#define VAXD_DBL_BIAS  0x81
#define VAXG_DBL_BIAS  0x401
#define IEEE_DBL_BIAS  0x3ff

void vax2ieee_single(float *f, unsigned int len)
	{
	float tmp;
	ieee_single ieee(f);
	vax_single  vax(&tmp);

	for (int i=0; i < len; ++i, ++ieee)
		{
		tmp = *ieee;
		if ( vax.exp() == vax.maxExp() && vax.mantissa() == vax.maxMantissa() )
			{
			ieee.exp(ieee.maxExp());
			ieee.mantissa(ieee.maxMantissa());
			}
		else
			{
			ieee.exp(vax.exp() - VAX_SNG_BIAS + IEEE_SNG_BIAS);
			ieee.mantissa(vax.mantissa());
			}
		ieee.sign(vax.sign());
		}
	}

void ieee2vax_single(float *f, unsigned int len)
	{
	float tmp;
	vax_single  vax(f);
	ieee_single ieee(&tmp);

	for (int i=0; i < len; ++i, ++vax)
		{
		tmp = *vax;
		if ( ieee.exp() == ieee.maxExp() && ieee.mantissa() == ieee.maxMantissa() )
			{
			vax.exp(vax.maxExp());
			vax.mantissa(vax.maxMantissa());
			}
		else
			{
			vax.exp(ieee.exp() - IEEE_SNG_BIAS + VAX_SNG_BIAS);
			vax.mantissa(ieee.mantissa());
			}
		vax.sign(ieee.sign());
		}
	}


void vax2ieee_double(double *f, unsigned int len,char op)
	{
	double tmp;
	ieee_double ieee(f);
	vax_double  vax(&tmp,op);

	for (int i=0; i < len; ++i, ++ieee)
		{
		tmp = *ieee;
		if ( vax.exp() == vax.maxExp() && vax.mantissa() == vax.maxMantissa() )
			{
			ieee.exp(ieee.maxExp());
			ieee.mantissa(ieee.maxMantissa());
			}
		else
			{
			ieee.exp(vax.exp() - (op == 'D' ? VAXD_DBL_BIAS : VAXG_DBL_BIAS) + IEEE_DBL_BIAS);
			ieee.mantissa(op == 'D' ? (vax.mantissa() >> 3) : vax.mantissa());
			}
		ieee.sign(vax.sign());
		}
	}

void ieee2vax_double(double *f, unsigned int len, char op)
	{
	double tmp;
	vax_double  vax(f,op);
	ieee_double ieee(&tmp);

	for (int i=0; i < len; ++i, ++vax)
		{
		tmp = *vax;
		if ( ieee.exp() == ieee.maxExp() && ieee.mantissa() == ieee.maxMantissa() )
			{
			vax.exp(vax.maxExp());
			vax.mantissa(vax.maxMantissa());
			}
		else
			{
			vax.exp(ieee.exp() - IEEE_DBL_BIAS + (op == 'D' ? VAXD_DBL_BIAS : VAXG_DBL_BIAS));
			vax.mantissa(op == 'D' ? (ieee.mantissa() << 3) : vax.mantissa());
			}
		vax.sign(ieee.sign());
		}
	}


