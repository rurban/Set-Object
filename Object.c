/*
 * This file was generated automatically by ExtUtils::ParseXS version 2.18_02 from the
 * contents of Object.xs. Do not edit this file, edit Object.xs instead.
 *
 *	ANY CHANGES MADE HERE WILL BE LOST! 
 *
 */

#line 1 "Object.xs"
#ifdef __cplusplus
extern "C" {
#endif
#define PERL_POLLUTE
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#include "ppport.h"

#if __GNUC__ >= 3   /* I guess. */
#define _warn(msg, e...) warn("# (" __FILE__ ":%d): " msg, __LINE__, ##e)
#else
#define _warn warn
#endif

// for debugging object-related functions
#define IF_DEBUG(e)

// for debugging scalar-related functions
#define IF_REMOVE_DEBUG(e)
#define IF_INSERT_DEBUG(e)

// for debugging weakref-related functions
#define IF_SPELL_DEBUG(e)

#define SET_OBJECT_MAGIC_backref (char)0x9f

typedef struct _BUCKET
{
	SV** sv;
	int n;
} BUCKET;

typedef struct _ISET
{
	BUCKET* bucket;
	I32 buckets, elems;
        SV* is_weak;
        HV* flat;
} ISET;

#define ISET_HASH(el) ((I32) (el) >> 4)

#define ISET_INSERT(s, item) \
	     ( SvROK(item) \
	       ? iset_insert_one(s, item) \
               : iset_insert_scalar(s, item) )

int insert_in_bucket(BUCKET* pb, SV* sv)
{
	if (!pb->sv)
	{
		New(0, pb->sv, 1, SV*);
		pb->sv[0] = sv;
		pb->n = 1;
		IF_DEBUG(_warn("inserting 0x%.8x in bucket 0x%.8x offset %d", sv, pb, 0));
	}
	else
	{
		SV **iter = pb->sv, **last = pb->sv + pb->n, **hole = 0;

		for (; iter != last; ++iter)
		{
			if (*iter)
			{
				if (*iter == sv)
					return 0;
			}
			else
				hole = iter;
		}

		if (!hole)
		{
			Renew(pb->sv, pb->n + 1, SV*);
			hole = pb->sv + pb->n;
			++pb->n;
		}

		*hole = sv;

		IF_DEBUG(_warn("inserting 0x%.8x in bucket 0x%.8x offset %d", sv, pb, iter - pb->sv));
	}
	
	return 1;
}

int iset_insert_scalar(ISET* s, SV* sv)
{
  STRLEN len;
  char* key = 0;

  if (!s->flat) {
    IF_INSERT_DEBUG(_warn("iset_insert_scalar(%x): creating scalar hash", s));
    s->flat = newHV();
  }

  if (!SvOK(sv))
     return 0;

  //SvGETMAGIC(sv);
  key = SvPV(sv, len);

  IF_INSERT_DEBUG(_warn("iset_insert_scalar(%x): sv (%x, rc = %d, str= '%s')!", s, sv, SvREFCNT(sv), SvPV_nolen(sv)));

  if (!hv_exists(s->flat, key, len)) {

    if (!hv_store(s->flat, key, len, &PL_sv_undef, 0)) {
      _warn("hv store failed[?] set=%x", s);
    }

    IF_INSERT_DEBUG(_warn("iset_insert_scalar(%x): inserted OK!", s));

    return 1;
  }
  else {
    
    IF_INSERT_DEBUG(_warn("iset_insert_scalar(%x): already there!", s));
    return 0;
  }

}

int iset_remove_scalar(ISET* s, SV* sv)
{
  STRLEN len;
  char* key = 0;

  if (!s->flat) {
    IF_REMOVE_DEBUG(_warn("iset_remove_scalar(%x): shortcut for %x(str = '%s') (no hash)", s, sv, SvPV_nolen(sv)));
    return 0;
  }

  //DEBUG("Checking for existance of %s", SvPV_nolen(sv));
  //SvGETMAGIC(sv);
  IF_REMOVE_DEBUG(_warn("iset_remove_scalar(%x): sv (%x, rc = %d, str= '%s')!", s, sv, SvREFCNT(sv), SvPV_nolen(sv)));

  key = SvPV(sv, len);

  if ( hv_delete(s->flat, key, len, 0) ) {

    IF_REMOVE_DEBUG(_warn("iset_remove_scalar(%x): deleted key", s));
    return 1;

  } else {

    IF_REMOVE_DEBUG(_warn("iset_remove_scalar(%x): key not absent", s));
    return 0;
  }
  
}

bool iset_includes_scalar(ISET* s, SV* sv)
{
  if (s->flat) {
    STRLEN len;
    char* key = SvPV(sv, len);
    return hv_exists(s->flat, key, len);
  }
  else {
    return 0;
  }
}

void _cast_magic(ISET* s, SV* sv);

int iset_insert_one(ISET* s, SV* rv)
{
	I32 hash, index;
	SV* el;
	int ins = 0;

	if (!SvROK(rv))
	{
	    Perl_croak(aTHX_ "Tried to insert a non-reference into a Set::Object");
	};

	el = SvRV(rv);

	if (!s->buckets)
	{
		Newz(0, s->bucket, 8, BUCKET);
		s->buckets = 8;
	}

	hash = ISET_HASH(el);
	index = hash & (s->buckets - 1);

	if (insert_in_bucket(s->bucket + index, el))
	{
		++s->elems;
		++ins;
		if (s->is_weak) {
		    IF_DEBUG(_warn("rc of 0x%.8x left as-is, casting magic", el));
		    _cast_magic(s, el);
		} else {
		    SvREFCNT_inc(el);
		    IF_DEBUG(_warn("rc of 0x%.8x bumped to %d", el, SvREFCNT(el)));
		}
	}

	if (s->elems > s->buckets)
	{
		int oldn = s->buckets;
		int newn = oldn << 1;

		BUCKET *bucket_first, *bucket_iter, *bucket_last, *new_bucket;
		int i;

		IF_DEBUG(_warn("Reindexing, n = %d", s->elems));

		Renew(s->bucket, newn, BUCKET);
		Zero(s->bucket + oldn, oldn, BUCKET);
		s->buckets = newn;

		bucket_first = s->bucket;
		bucket_iter = bucket_first;
		bucket_last = bucket_iter + oldn;

		for (i = 0; bucket_iter != bucket_last; ++bucket_iter, ++i)
		{
			SV **el_iter, **el_last, **el_out_iter;
			I32 new_bucket_size;

			if (!bucket_iter->sv)
				continue;

			el_iter = bucket_iter->sv;
			el_last = el_iter + bucket_iter->n;
			el_out_iter = el_iter;

			for (; el_iter != el_last; ++el_iter)
			{
				SV* sv = *el_iter;
				I32 hash = ISET_HASH(sv);
				I32 index = hash & (newn - 1);

				if (index == i)
				{
					*el_out_iter++ = *el_iter;
					continue;
				}

				new_bucket = bucket_first + index;
				IF_DEBUG(_warn("0x%.8x moved from bucket %d:0x%.8x to %d:0x%.8x",
					       sv, i, bucket_iter, index, new_bucket));
				insert_in_bucket(new_bucket, sv);
			}
         
			new_bucket_size = el_out_iter - bucket_iter->sv;

			if (!new_bucket_size)
			{
				Safefree(bucket_iter->sv);
				bucket_iter->sv = 0;
				bucket_iter->n = 0;
			}

			else if (new_bucket_size < bucket_iter->n)
			{
				Renew(bucket_iter->sv, new_bucket_size, SV*);
				bucket_iter->n = new_bucket_size;
			}
		}
	}

	return ins;
}

void _dispel_magic(ISET* s, SV* sv);

void iset_clear(ISET* s)
{
	BUCKET* bucket_iter = s->bucket;
	BUCKET* bucket_last = bucket_iter + s->buckets;

	for (; bucket_iter != bucket_last; ++bucket_iter)
	{
		SV **el_iter, **el_last;

		if (!bucket_iter->sv)
            continue;

		el_iter = bucket_iter->sv;
		el_last = el_iter + bucket_iter->n;

		for (; el_iter != el_last; ++el_iter)
		{
			if (*el_iter)
			{
				IF_DEBUG(_warn("freeing 0x%.8x, rc = %d, bucket = 0x%.8x(%d)) pos = %d",
					 *el_iter, SvREFCNT(*el_iter),
					 bucket_iter, bucket_iter - s->bucket,
					 el_iter - bucket_iter->sv));

				if (s->is_weak) {
				  IF_SPELL_DEBUG(_warn("dispelling magic"));
				  _dispel_magic(s,*el_iter);
				} else {
				  IF_SPELL_DEBUG(_warn("removing element"));
				  SvREFCNT_dec(*el_iter);
				}
				*el_iter = 0;
			}
		}

		Safefree(bucket_iter->sv);

		bucket_iter->sv = 0;
		bucket_iter->n = 0;
	}

	Safefree(s->bucket);
	s->bucket = 0;
	s->buckets = 0;
	s->elems = 0;
}


MAGIC*
_detect_magic(SV* sv) {
    return mg_find(sv, SET_OBJECT_MAGIC_backref);
}

void
_dispel_magic(ISET* s, SV* sv) {
    SV* self_svrv = s->is_weak;
    MAGIC* mg = _detect_magic(sv);
    IF_SPELL_DEBUG(_warn("dispelling magic from 0x%.8x (self = 0x%.8x, mg = 0x%.8x)",
			 sv, self_svrv, mg));
    if (mg) {
       AV* wand = mg->mg_obj;
       SV ** const svp = AvARRAY(wand);
       I32 i = AvFILLp(wand);
       int c = 0;

       while (i >= 0) {
	 if (svp[i] && SvIV(svp[i])) {
	   ISET* o = INT2PTR(ISET*, SvIV(svp[i]));
	   if (s == o) {
	     /*
	     SPELL_DEBUG("dropping RC of 0x%.8x from %d to %d",
			 svp[i], SvREFCNT(svp[i]), SvREFCNT(svp[i])-1);
	     SvREFCNT_dec(svp[i]);
	     */
	     svp[i] = newSViv(0);
	   } else {
	     c++;
	   }
	 }
	 i--;
       }
       if (!c) {
	 /* we should clear the magic, really. */
	 MAGIC* last = 0;
	 for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	   if (mg->mg_type == SET_OBJECT_MAGIC_backref) {
	     if (last) {
	       last->mg_moremagic = mg->mg_moremagic;
	       Safefree(mg);
	       break;
	     } else if (mg->mg_moremagic) {
	       SvMAGIC(sv) = mg->mg_moremagic;
	     } else {
	       SvMAGIC(sv) = 0;
	       SvAMAGIC_off(sv);
	     }
	   }
	   last=mg;
	 }
       }
    }
}

void
_fiddle_strength(ISET* s, int strong) {

      BUCKET* bucket_iter = s->bucket;
      BUCKET* bucket_last = bucket_iter + s->buckets;

      for (; bucket_iter != bucket_last; ++bucket_iter)
      {
         SV **el_iter, **el_last;

         if (!bucket_iter->sv)
            continue;

         el_iter = bucket_iter->sv;
         el_last = el_iter + bucket_iter->n;

         for (; el_iter != el_last; ++el_iter)
            if (*el_iter) {
	      if (strong) {
		_dispel_magic(s, *el_iter);
		SvREFCNT_inc(*el_iter);
		IF_DEBUG(_warn("bumped RC of 0x%.8x to %d", *el_iter,
			       SvREFCNT(*el_iter)));
	      }
	      else {
		_cast_magic(s, *el_iter);
		SvREFCNT_dec(*el_iter);
		IF_DEBUG(_warn("reduced RC of 0x%.8x to %d", *el_iter,
			       SvREFCNT(*el_iter)));
	      }
	    }
      }
}

int
_spell_effect(pTHX_ SV *sv, MAGIC *mg)
{
    AV * const av = (AV*)mg->mg_obj;
    SV ** const svp = AvARRAY(av);
    I32 i = AvFILLp(av);

    IF_SPELL_DEBUG(_warn("_spell_effect (SV=0x%.8x, av_len=%d)", sv,
			 av_len(av)));

    while (i >= 0) {
        IF_SPELL_DEBUG(_warn("_spell_effect %d", i));
	if (svp[i] && SvIV(svp[i])) {
	  ISET* s = INT2PTR(ISET*, SvIV(svp[i]));
	  IF_SPELL_DEBUG(_warn("_spell_effect i = %d, SV = 0x%.8x", i, svp[i]));
	  if (!s->is_weak)
	    Perl_croak(aTHX_ "panic: set_object_magic_killbackrefs (flags=%"UVxf")",
		       (UV)SvFLAGS(svp[i]));
	  /* SvREFCNT_dec(svp[i]); */
	  svp[i] = newSViv(0);
	  if (iset_remove_one(s, sv, 1) != 1) {
	    _warn("Set::Object magic backref hook called on non-existent item (0x%x, self = 0x%x)", sv, s->is_weak);
	  };
	}
	i--;
    }
}

static MGVTBL SET_OBJECT_vtbl_backref =
 	  {0,	0, 0,	0, MEMBER_TO_FPTR(_spell_effect)};

void
_cast_magic(ISET* s, SV* sv) {
    SV* self_svrv = s->is_weak;
    AV* wand;
    MGVTBL *vtable = &SET_OBJECT_vtbl_backref;
    MAGIC* mg;
    SV ** svp;
    int how = 0;
    I32 i,l,free;
    how = 0x9f; // (int)SET_OBJECT_MAGIC_backref;

    mg = _detect_magic(sv);
    if (mg) {
      IF_SPELL_DEBUG(_warn("sv_magicext reusing wand 0x%.8x for 0x%.8x", wand, sv));
      wand = mg->mg_obj;
    }
    else {
      wand=newAV();
      IF_SPELL_DEBUG(_warn("sv_magicext(0x%.8x, 0x%.8x, %ld, 0x%.8x, NULL, 0)", sv, wand, how, vtable));
      sv_magicext(sv, wand, how, vtable, NULL, 0);
      SvRMAGICAL_on(sv);
    }

    svp = AvARRAY(wand);
    i = AvFILLp(wand);
    free = -1;

    while (i >= 0) {
      if (svp[i] && SvIV(svp[i])) {
	ISET* o = INT2PTR(ISET*, SvIV(svp[i]));
	if (s == o)
	  return;
      } else {
	free = i;
      }
      i = i - 1;
    }

    if (free == -1) {
      IF_SPELL_DEBUG(_warn("casting self 0x%.8x with av_push", self_svrv, free));
      av_push(wand, self_svrv);
    } else {
      IF_SPELL_DEBUG(_warn("casting self 0x%.8x to slot %d", self_svrv, free));
      svp[free] = self_svrv;
    }
    /*
    SvREFCNT_inc(self_svrv);
    */
}

int
iset_remove_one(ISET* s, SV* el, int spell_in_progress)
{
  SV *referant;
      I32 hash, index;
      SV **el_iter, **el_last, **el_out_iter;
      BUCKET* bucket;

  IF_DEBUG(_warn("removing scalar 0x%.8x from set 0x%.8x", el, s));

  /* note an object being destroyed is not SvOK */
  if (!spell_in_progress && !SvOK(el))
    return 0;

  if (SvOK(el) && !SvROK(el)) {
    IF_DEBUG(_warn("scalar is not a ref (flags = 0x%.8x)", SvFLAGS(el)));
    if (s->flat) {
      IF_DEBUG(_warn("calling remove_scalar for 0x%.8x", el));
      if (iset_remove_scalar(s, el))
	return 1;
    }
    return 0;
  }

  referant = (spell_in_progress ? el : SvRV(el));
  hash = ISET_HASH(referant);
  index = hash & (s->buckets - 1);
  bucket = s->bucket + index;

  if (s->buckets == 0)
    return 0;

  if (!bucket->sv)
    return 0;

  el_iter = bucket->sv;
  el_out_iter = el_iter;
  el_last = el_iter + bucket->n;
  IF_DEBUG(_warn("remove: el_last = 0x%.8x, el_iter = 0x%.8x", el_last, el_iter));

  for (; el_iter != el_last; ++el_iter)
    {
      if (*el_iter == referant)
	{
	  if (s->is_weak) {
	    if (!spell_in_progress) {
	      IF_SPELL_DEBUG(_warn("Removing ST(0x%.8x) magic", referant));
	      _dispel_magic(s,referant);
	    } else {
	      IF_SPELL_DEBUG(_warn("Not removing ST(0x%.8x) magic (spell in progress)", referant));

	    }
	  } else {
	    IF_SPELL_DEBUG(_warn("Not removing ST(0x%.8x) magic from Muggle", referant));
	    SvREFCNT_dec(referant);
	  }
	  *el_iter = 0;
	  --s->elems;
	  return 1;
	}
      else
	{
	  IF_SPELL_DEBUG(_warn("ST(0x%.8x) != 0x%.8x", referant, *el_iter));
	}
    }
  return 0;
}
  
#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(var) if (0) var = var
#endif

#line 576 "Object.c"

XS(XS_Set__Object_new); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_new)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items < 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::new", "pkg, ...");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	pkg = ST(0);
#line 572 "Object.xs"
   {
	   SV* self;
	   ISET* s;
	   I32 item;
	   SV* isv;

	   New(0, s, 1, ISET);
	   //_warn("created set id = %x", s);
	   s->elems = 0;
	   s->bucket = 0;
	   s->buckets = 0;
	   s->flat = 0;
	   s->is_weak = 0;

	   // _warning: cast from pointer to integer of different size
	   isv = newSViv( PTR2IV(s) );
	   sv_2mortal(isv);

	   self = newRV_inc(isv);
	   sv_2mortal(self);

	   sv_bless(self, gv_stashsv(pkg, FALSE));

	   for (item = 1; item < items; ++item)
	   {
		   ISET_INSERT(s, ST(item));
	   }

      IF_DEBUG(_warn("set!"));

      PUSHs(self);
      XSRETURN(1);
   }
#line 627 "Object.c"
	PUTBACK;
	return;
    }
}


XS(XS_Set__Object_insert); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_insert)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items < 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::insert", "self, ...");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	self = ST(0);
#line 611 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));
      I32 item;
      int inserted = 0;

      for (item = 1; item < items; ++item)
      {
	if ((SV*)s == ST(item)) {
	  _warn("INSERTING SET UP OWN ARSE");
	}
	if ISET_INSERT(s, ST(item))
			inserted++;
		  IF_DEBUG(_warn("inserting 0x%.8x 0x%.8x size = %d", ST(item), SvRV(ST(item)), s->elems));
      }


      XSRETURN_IV(inserted);
#line 666 "Object.c"
	PUTBACK;
	return;
    }
}


XS(XS_Set__Object_remove); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_remove)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items < 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::remove", "self, ...");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	self = ST(0);
#line 634 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));
      I32 hash, index, item;
      SV **el_iter, **el_last, **el_out_iter;
      BUCKET* bucket;
      int removed = 0;

      for (item = 1; item < items; ++item)
      {
         SV* el = ST(item);

	 removed += iset_remove_one(s, el, 0);
      }
remove_out:
      XSRETURN_IV(removed);
#line 703 "Object.c"
	PUTBACK;
	return;
    }
}


XS(XS_Set__Object_is_null); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_null)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_null", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
	int	RETVAL;
	dXSTARG;
#line 654 "Object.xs"
   ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

   if (s->elems)
     XSRETURN_UNDEF;

   if (s->flat) {
     if (HvKEYS(s->flat)) {
       //_warn("got some keys: %d\n", HvKEYS(s->flat));
       XSRETURN_UNDEF;
     }
   }

   RETVAL = 1;

#line 740 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_size); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_size)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::size", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
	int	RETVAL;
	dXSTARG;
#line 675 "Object.xs"
   ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

   RETVAL = s->elems + (s->flat ? HvKEYS(s->flat) : 0);


#line 768 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_rc); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_rc)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::rc", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
	int	RETVAL;
	dXSTARG;
#line 688 "Object.xs"
      RETVAL = SvREFCNT(self);

#line 793 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_rvrc); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_rvrc)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::rvrc", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
	int	RETVAL;
	dXSTARG;
#line 698 "Object.xs"
   if (SvROK(self)) {
     RETVAL = SvREFCNT(SvRV(self));
   } else {
     XSRETURN_UNDEF;
   }

#line 822 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_includes); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_includes)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items < 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::includes", "self, ...");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	self = ST(0);
#line 712 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));
      I32 hash, index, item;
      SV **el_iter, **el_last;
      BUCKET* bucket;

      for (item = 1; item < items; ++item)
      {
         SV* el = ST(item);
         SV* rv;

	 if (!SvOK(el))
	   XSRETURN_NO;

	 if (!SvROK(el)) {
	   IF_DEBUG(_warn("includes! el = %s", SvPV_nolen(el)));
	   if (!iset_includes_scalar(s, el))
	     XSRETURN_NO;
	   goto next;
	 }

	 rv = SvRV(el);

         if (!s->buckets)
            XSRETURN_NO;

         hash = ISET_HASH(rv);
         index = hash & (s->buckets - 1);
         bucket = s->bucket + index;

	 IF_DEBUG(_warn("includes: looking for 0x%.8x in bucket %d:0x%.8x",
			rv, index, bucket));

         if (!bucket->sv)
            XSRETURN_NO;

         el_iter = bucket->sv;
         el_last = el_iter + bucket->n;

         for (; el_iter != el_last; ++el_iter)
            if (*el_iter == rv)
               goto next;

         XSRETURN_NO;

         next: ;
      }

      XSRETURN_YES;
#line 893 "Object.c"
	PUTBACK;
	return;
    }
}


XS(XS_Set__Object_members); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_members)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::members", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	self = ST(0);
#line 768 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));
      BUCKET* bucket_iter = s->bucket;
      BUCKET* bucket_last = bucket_iter + s->buckets;

      EXTEND(sp, s->elems + (s->flat ? HvKEYS(s->flat) : 0) );

      for (; bucket_iter != bucket_last; ++bucket_iter)
      {
         SV **el_iter, **el_last;

         if (!bucket_iter->sv)
            continue;

         el_iter = bucket_iter->sv;
         el_last = el_iter + bucket_iter->n;

         for (; el_iter != el_last; ++el_iter)
            if (*el_iter)
			{
				SV* el = newRV(*el_iter);
				if (SvOBJECT(*el_iter)) {
				  sv_bless(el, SvSTASH(*el_iter));
				}
				PUSHs(sv_2mortal(el));
				//XPUSHs(el);
				//PUSHs(el);
			}
      }

      if (s->flat) {
        int i = 0, num = hv_iterinit(s->flat);

        while (i++ < num) {
	  HE* he = hv_iternext(s->flat);

	  PUSHs(HeSVKEY_force(he));
        }
      }
//_warn("that's all, folks");
#line 955 "Object.c"
	PUTBACK;
	return;
    }
}


XS(XS_Set__Object_clear); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_clear)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::clear", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
#line 813 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

      iset_clear(s);
      if (s->flat) {
	hv_clear(s->flat);
	IF_REMOVE_DEBUG(_warn("iset_clear(%x): cleared", s));
      }
#line 983 "Object.c"
    }
    XSRETURN_EMPTY;
}


XS(XS_Set__Object_DESTROY); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_DESTROY)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::DESTROY", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
#line 826 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));
      IF_DEBUG(_warn("aargh!"));
      iset_clear(s);
      if (s->flat) {
	hv_undef(s->flat);
	SvREFCNT_dec(s->flat);
      }
      Safefree(s);
#line 1011 "Object.c"
    }
    XSRETURN_EMPTY;
}


XS(XS_Set__Object_is_weak); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_weak)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_weak", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
	int	RETVAL;
	dXSTARG;
#line 840 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

      RETVAL = !!s->is_weak;

#line 1037 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object__weaken); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object__weaken)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::_weaken", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
#line 851 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

      if (s->is_weak)
        XSRETURN_UNDEF;

	IF_DEBUG(_warn("weakening set (0x%.8x)", SvRV(self)));

      s->is_weak = SvRV(self);

      _fiddle_strength(s, 0);
#line 1068 "Object.c"
    }
    XSRETURN_EMPTY;
}


XS(XS_Set__Object__strengthen); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object__strengthen)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::_strengthen", "self");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	self = ST(0);
#line 867 "Object.xs"
      ISET* s = INT2PTR(ISET*, SvIV(SvRV(self)));

      if (!s->is_weak)
        XSRETURN_UNDEF;

	IF_DEBUG(_warn("strengthening set (0x%.8x)", SvRV(self)));

      _fiddle_strength(s, 1);

      s->is_weak = 0;

   /* Here are some functions from Scalar::Util; they are so simple,
      that it isn't worth making a dependancy on that module. */
#line 1101 "Object.c"
    }
    XSRETURN_EMPTY;
}


XS(XS_Set__Object_is_int); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_int)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_int", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 886 "Object.xs"
  SvGETMAGIC(sv);
  if ( !SvIOKp(sv) )
     XSRETURN_UNDEF;

  RETVAL = 1;
#line 1128 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_is_string); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_string)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_string", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 899 "Object.xs"
  SvGETMAGIC(sv);
  if ( !SvPOKp(sv) )
     XSRETURN_UNDEF;

  RETVAL = 1;
#line 1156 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_is_double); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_double)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_double", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 912 "Object.xs"
  SvGETMAGIC(sv);
  if ( !SvNOKp(sv) )
     XSRETURN_UNDEF;

  RETVAL = 1;
#line 1184 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_get_magic); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_get_magic)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::get_magic", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
#line 925 "Object.xs"
  MAGIC* mg;
  SV* magic;
  if (! SvROK(sv)) {
     _warn("tried to get magic from non-reference");
     XSRETURN_UNDEF;
  }

  if (! (mg = _detect_magic(SvRV(sv))) )
     XSRETURN_UNDEF;

  IF_SPELL_DEBUG(_warn("found magic on 0x%.8x - 0x%.8x", sv, mg));
  IF_SPELL_DEBUG(_warn("mg_obj = 0x%.8x", mg->mg_obj));

     /*magic = newSV(0);
  SvRV(magic) = mg->mg_obj;
  SvROK_on(magic); */
  POPs;
  magic = newRV_inc(mg->mg_obj);
  PUSHs(magic);
  XSRETURN(1);
#line 1225 "Object.c"
    }
    XSRETURN_EMPTY;
}


XS(XS_Set__Object_get_flat); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_get_flat)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::get_flat", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV*	sv = ST(0);
	SV *	RETVAL;
#line 951 "Object.xs"
  ISET* s = INT2PTR(ISET*, SvIV(SvRV(sv)));
  if (s->flat) {
    RETVAL = newRV_inc(s->flat);
  } else {
    XSRETURN_UNDEF;
  }
#line 1252 "Object.c"
	ST(0) = RETVAL;
	sv_2mortal(ST(0));
    }
    XSRETURN(1);
}


XS(XS_Set__Object_blessed); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_blessed)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::blessed", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	char *	RETVAL;
	dXSTARG;
#line 965 "Object.xs"
{
    if (SvMAGICAL(sv))
	mg_get(sv);
    if(!sv_isobject(sv)) {
	XSRETURN_UNDEF;
    }
    RETVAL = sv_reftype(SvRV(sv),TRUE);
}
#line 1284 "Object.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS(XS_Set__Object_reftype); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_reftype)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::reftype", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	char *	RETVAL;
	dXSTARG;
#line 981 "Object.xs"
{
    if (SvMAGICAL(sv))
	mg_get(sv);
    if(!SvROK(sv)) {
	XSRETURN_UNDEF;
    }
    RETVAL = sv_reftype(SvRV(sv),FALSE);
}
#line 1315 "Object.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS(XS_Set__Object_refaddr); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_refaddr)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::refaddr", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	UV	RETVAL;
	dXSTARG;
#line 997 "Object.xs"
{
    if(SvROK(sv)) {
      // Don't return undef if not a valid ref - return 0 instead
      // (less "Use of uninitialised value..." messages)

      // XSRETURN_UNDEF;
	RETVAL = PTR2UV(SvRV(sv));
    } else {
      RETVAL = 0;
    }
}
#line 1349 "Object.c"
	XSprePUSH; PUSHu((UV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object__ish_int); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object__ish_int)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::_ish_int", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 1017 "Object.xs"
  double dutch;
  int innit;
  STRLEN lp;  // world famous in NZ
  SV * MH;
  // This function returns the integer value of a passed scalar, as
  // long as the scalar can reasonably considered to already be a
  // representation of an integer.  This means if you want strings to
  // be interpreted as integers, you're going to have to add 0 to
  // them.

  if (SvMAGICAL(sv)) {
    // probably a tied scalar
    //mg_get(sv);
    Perl_croak(aTHX_ "Tied variables not supported");
  }

  if (SvAMAGIC(sv)) {
    // an overloaded variable.  need to actually call a function to
    // get its value.
    Perl_croak(aTHX_ "Overloaded variables not supported");
  }

  if (SvNIOKp(sv)) {
    // NOK - the scalar is a double

    if (SvPOKp(sv)) {
      // POK - the scalar is also a string.

      // we have to be careful; a scalar "2am" or, even worse, "2e6"
      // may satisfy this condition if it has been evaluated in
      // numeric context.  Remember, we are testing that the value
      // could already be considered an _integer_, and AFAIC 2e6 and
      // 2.0 are floats, end of story.

      // So, we stringify the numeric part of the passed SV, turn off
      // the NOK bit on the scalar, so as to perform a string
      // comparison against the passed in value.  If it is not the
      // same, then we almost certainly weren't given an integer.

      if (SvIOKp(sv)) {
	MH = newSViv(SvIV(sv));
      } else if (SvNOKp(sv)) {
	MH = newSVnv(SvNV(sv));
      }
      sv_2pv(MH, &lp);
      SvPOK_only(MH);

      if (sv_cmp(MH, sv) != 0) {
	XSRETURN_UNDEF;
      }
    }

    if (SvNOKp(sv)) {
      // How annoying - it's a double
      dutch = SvNV(sv);
      if (SvIOKp(sv)) {
	innit = SvIV(sv);
      } else {
	innit = (int)dutch;
      }
      if (dutch - innit < (0.000000001)) {
	RETVAL = innit;
      } else {
	XSRETURN_UNDEF;
      }
    } else if (SvIOKp(sv)) {
      RETVAL = SvIV(sv);
    }
  } else {
    XSRETURN_UNDEF;
  }
#line 1443 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_is_overloaded); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_overloaded)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_overloaded", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 1096 "Object.xs"
  SvGETMAGIC(sv);
  if ( !SvAMAGIC(sv) )
     XSRETURN_UNDEF;
  RETVAL = 1;
#line 1470 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object_is_object); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object_is_object)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::is_object", "sv");
    PERL_UNUSED_VAR(cv); /* -W */
    {
	SV *	sv = ST(0);
	int	RETVAL;
	dXSTARG;
#line 1108 "Object.xs"
  SvGETMAGIC(sv);
  if ( !SvOBJECT(sv) )
     XSRETURN_UNDEF;
  RETVAL = 1;
#line 1497 "Object.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Set__Object__STORABLE_thaw); /* prototype to pass -Wmissing-prototypes */
XS(XS_Set__Object__STORABLE_thaw)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items < 3)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Set::Object::_STORABLE_thaw", "obj, cloning, serialized, ...");
    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
	SV*	obj = ST(0);
#line 1121 "Object.xs"
   {
	   ISET* s;
	   I32 item;
	   SV* isv;

	   New(0, s, 1, ISET);
	   s->elems = 0;
	   s->bucket = 0;
	   s->buckets = 0;
	   s->flat = 0;
	   s->is_weak = 0;

	   if (!SvROK(obj)) {
	     Perl_croak(aTHX_ "Set::Object::STORABLE_thaw passed a non-reference");
	   }

	   /* FIXME - some random segfaults with 5.6.1, Storable 2.07,
		      freezing closures, and back-references to
		      overloaded objects.  One day I might even
		      understand why :-)

		      Bug in Storable... that's why.  old news.
	    */
	   isv = SvRV(obj);
	   SvIV_set(isv, PTR2IV(s) );
	   SvIOK_on(isv);

	   for (item = 3; item < items; ++item)
	   {
		  ISET_INSERT(s, ST(item));
	   }

      IF_DEBUG(_warn("set!"));

      PUSHs(obj);
      XSRETURN(1);
   }
#line 1557 "Object.c"
	PUTBACK;
	return;
    }
}

#ifdef __cplusplus
extern "C"
#endif
XS(boot_Set__Object); /* prototype to pass -Wmissing-prototypes */
XS(boot_Set__Object)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    char* file = __FILE__;

    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(items); /* -W */
    XS_VERSION_BOOTCHECK ;

        newXS("Set::Object::new", XS_Set__Object_new, file);
        newXS("Set::Object::insert", XS_Set__Object_insert, file);
        newXS("Set::Object::remove", XS_Set__Object_remove, file);
        newXS("Set::Object::is_null", XS_Set__Object_is_null, file);
        newXS("Set::Object::size", XS_Set__Object_size, file);
        newXS("Set::Object::rc", XS_Set__Object_rc, file);
        newXS("Set::Object::rvrc", XS_Set__Object_rvrc, file);
        newXS("Set::Object::includes", XS_Set__Object_includes, file);
        newXS("Set::Object::members", XS_Set__Object_members, file);
        newXS("Set::Object::clear", XS_Set__Object_clear, file);
        newXS("Set::Object::DESTROY", XS_Set__Object_DESTROY, file);
        newXS("Set::Object::is_weak", XS_Set__Object_is_weak, file);
        newXS("Set::Object::_weaken", XS_Set__Object__weaken, file);
        newXS("Set::Object::_strengthen", XS_Set__Object__strengthen, file);
        newXSproto("Set::Object::is_int", XS_Set__Object_is_int, file, "$");
        newXSproto("Set::Object::is_string", XS_Set__Object_is_string, file, "$");
        newXSproto("Set::Object::is_double", XS_Set__Object_is_double, file, "$");
        newXSproto("Set::Object::get_magic", XS_Set__Object_get_magic, file, "$");
        newXSproto("Set::Object::get_flat", XS_Set__Object_get_flat, file, "$");
        newXSproto("Set::Object::blessed", XS_Set__Object_blessed, file, "$");
        newXSproto("Set::Object::reftype", XS_Set__Object_reftype, file, "$");
        newXSproto("Set::Object::refaddr", XS_Set__Object_refaddr, file, "$");
        newXSproto("Set::Object::_ish_int", XS_Set__Object__ish_int, file, "$");
        newXSproto("Set::Object::is_overloaded", XS_Set__Object_is_overloaded, file, "$");
        newXSproto("Set::Object::is_object", XS_Set__Object_is_object, file, "$");
        newXS("Set::Object::_STORABLE_thaw", XS_Set__Object__STORABLE_thaw, file);
    if (PL_unitcheckav)
         call_list(PL_scopestack_ix, PL_unitcheckav);
    XSRETURN_YES;
}

