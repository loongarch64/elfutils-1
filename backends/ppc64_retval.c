/* Function return value location for Linux/PPC64 ABI.
   Copyright (C) 2005 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <dwarf.h>

#define BACKEND ppc64_
#include "libebl_CPU.h"


/* r3.  */
static const Dwarf_Op loc_intreg[] =
  {
    { .atom = DW_OP_reg3 }
  };
#define nloc_intreg	1

/* f1, or f1:f2, or f1:f4.  */
static const Dwarf_Op loc_fpreg[] =
  {
    { .atom = DW_OP_regx, .number = 33 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 34 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 35 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 36 }, { .atom = DW_OP_piece, .number = 8 },
  };
#define nloc_fpreg	1
#define nloc_fp2regs	4
#define nloc_fp4regs	8

/* The return value is a structure and is actually stored in stack space
   passed in a hidden argument by the caller.  But, the compiler
   helpfully returns the address of that space in r3.  */
static const Dwarf_Op loc_aggregate[] =
  {
    { .atom = DW_OP_breg3, .number = 0 }
  };
#define nloc_aggregate 1

int
ppc64_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
{
  /* Start with the function's type, and get the DW_AT_type attribute,
     which is the type of the return value.  */

  Dwarf_Attribute attr_mem;
  Dwarf_Attribute *attr = dwarf_attr (functypedie, DW_AT_type, &attr_mem);
  if (attr == NULL)
    /* The function has no return value, like a `void' function in C.  */
    return 0;

  Dwarf_Die die_mem;
  Dwarf_Die *typedie = dwarf_formref_die (attr, &die_mem);
  int tag = dwarf_tag (typedie);

  /* Follow typedefs and qualifiers to get to the actual type.  */
  while (tag == DW_TAG_typedef
	 || tag == DW_TAG_const_type || tag == DW_TAG_volatile_type
	 || tag == DW_TAG_restrict_type || tag == DW_TAG_mutable_type)
    {
      attr = dwarf_attr (typedie, DW_AT_type, &attr_mem);
      typedie = dwarf_formref_die (attr, &die_mem);
      tag = dwarf_tag (typedie);
    }

  Dwarf_Word size;
  switch (tag)
    {
    case -1:
      return -1;

    case DW_TAG_subrange_type:
      if (! dwarf_hasattr (typedie, DW_AT_byte_size))
	{
	  attr = dwarf_attr (typedie, DW_AT_type, &attr_mem);
	  typedie = dwarf_formref_die (attr, &die_mem);
	  tag = dwarf_tag (typedie);
	}
      /* Fall through.  */

    case DW_TAG_base_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_pointer_type:
    case DW_TAG_ptr_to_member_type:
      if (dwarf_formudata (dwarf_attr (typedie, DW_AT_byte_size,
				       &attr_mem), &size) != 0)
	{
	  if (tag == DW_TAG_pointer_type || tag == DW_TAG_ptr_to_member_type)
	    size = 8;
	  else
	    return -1;
	}
      if (tag == DW_TAG_base_type)
	{
	  Dwarf_Word encoding;
	  if (dwarf_formudata (dwarf_attr (typedie, DW_AT_encoding,
					   &attr_mem), &encoding) != 0)
	    return -1;

	  if (encoding == DW_ATE_float || encoding == DW_ATE_complex_float)
	    {
	      *locp = loc_fpreg;
	      if (size <= 8)
		return nloc_fpreg;
	      if (size <= 16)
		return nloc_fp2regs;
	      if (size <= 32)
		return nloc_fp4regs;
	    }
	}
      if (size <= 8)
	{
	intreg:
	  *locp = loc_intreg;
	  return nloc_intreg;
	}

      /* Else fall through.  */
    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:
    aggregate:
      *locp = loc_aggregate;
      return nloc_aggregate;

    case DW_TAG_string_type:
    case DW_TAG_array_type:
      if (dwarf_formudata (dwarf_attr (typedie, DW_AT_byte_size,
				       &attr_mem), &size) == 0
	  && size <= 8)
	{
	  if (tag == DW_TAG_array_type)
	    {
	      /* Check if it's a character array.  */
	      attr = dwarf_attr (typedie, DW_AT_type, &attr_mem);
	      typedie = dwarf_formref_die (attr, &die_mem);
	      tag = dwarf_tag (typedie);
	      if (tag != DW_TAG_base_type)
		goto aggregate;
	      if (dwarf_formudata (dwarf_attr (typedie, DW_AT_byte_size,
					       &attr_mem), &size) != 0)
		return -1;
	      if (size != 1)
		goto aggregate;
	    }
	  goto intreg;
	}
      goto aggregate;
    }

  /* XXX We don't have a good way to return specific errors from ebl calls.
     This value means we do not understand the type, but it is well-formed
     DWARF and might be valid.  */
  return -2;
}