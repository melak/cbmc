/*******************************************************************\

Module: C++ Language Type Checking

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <util/config.h>
#include <util/simplify_expr.h>
#include <util/arith_tools.h>
#include <util/std_types.h>
#include <util/i2string.h>
#include <util/expr_util.h>
#include <util/pointer_offset_size.h>

#include "c_typecheck_base.h"
#include "c_types.h"
#include "c_sizeof.h"
#include "c_qualifiers.h"
#include "ansi_c_declaration.h"
#include "padding.h"
#include "type2name.h"
#include "ansi_c_convert_type.h"

/*******************************************************************\

Function: c_typecheck_baset::typecheck_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_type(typet &type)
{
  // we first convert, and then check
  {
    ansi_c_convert_typet ansi_c_convert_type(get_message_handler());

    ansi_c_convert_type.read(type);
    ansi_c_convert_type.write(type);
  }
  
  if(type.id()==ID_already_typechecked)
  {
    // need to preserve any qualifiers
    c_qualifierst c_qualifiers(type);
    c_qualifiers+=c_qualifierst(type.subtype());
    bool packed=type.get_bool(ID_C_packed);
    type.swap(type.subtype());
    c_qualifiers.write(type);
    if(packed) type.set(ID_C_packed, true);
    return; // done
  }

  // do we have alignment?
  if(type.find(ID_C_alignment).is_not_nil())
  {
    exprt &alignment=static_cast<exprt &>(type.add(ID_C_alignment));
    if(alignment.id()!=ID_default)
    {
      typecheck_expr(alignment);
      make_constant(alignment);
    }
  }

  if(type.id()==ID_code)
    typecheck_code_type(to_code_type(type));
  else if(type.id()==ID_array)
    typecheck_array_type(to_array_type(type));
  else if(type.id()==ID_pointer)
    typecheck_type(type.subtype());
  else if(type.id()==ID_struct ||
          type.id()==ID_union)
    typecheck_compound_type(to_struct_union_type(type));
  else if(type.id()==ID_c_enum)
    typecheck_c_enum_type(type);
  else if(type.id()==ID_c_bitfield)
    typecheck_c_bit_field_type(type);
  else if(type.id()==ID_typeof)
    typecheck_typeof_type(type);
  else if(type.id()==ID_symbol)
    typecheck_symbol_type(type);
  else if(type.id()==ID_vector)
    typecheck_vector_type(to_vector_type(type));
  else if(type.id()==ID_custom_unsignedbv ||
          type.id()==ID_custom_signedbv ||
          type.id()==ID_custom_floatbv ||
          type.id()==ID_custom_fixedbv)
    typecheck_custom_type(type);
  else if(type.id()==ID_gcc_attribute_mode)
  {
    // A list of all modes ist at
    // http://www.delorie.com/gnu/docs/gcc/gccint_53.html
    typecheck_type(type.subtype());
    
    bool is_signed=type.subtype().id()==ID_signedbv;
    
    // get width
    irep_idt mode=type.get(ID_size);
    
    if(mode=="__QI__") // 8 bits
      type=is_signed?signed_char_type():unsigned_char_type();
    else if(mode=="__HI__") // 16 bits
      type=is_signed?signed_short_int_type():unsigned_short_int_type();
    else if(mode=="__SI__") // 32 bits
      type=is_signed?signed_int_type():unsigned_int_type();
    else if(mode=="__DI__") // 64 bits
    {
      if(config.ansi_c.long_int_width==64)
        type=is_signed?signed_long_int_type():unsigned_long_int_type();
      else
      {
        assert(config.ansi_c.long_long_int_width==64);
        type=is_signed?signed_long_long_int_type():unsigned_long_long_int_type();
      }
    }
    else if(mode=="__TI__") // 128 bits
      type=is_signed?gcc_signed_int128_type():gcc_unsigned_int128_type();
    else // give up, use base
      type=type.subtype();
  }

  // do a bit of rule checking

  if(type.get_bool(ID_C_restricted) &&
     type.id()!=ID_pointer &&
     type.id()!=ID_array)
  {
    err_location(type);
    error("only a pointer can be 'restrict'");
    throw 0;
  }
  
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_custom_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_custom_type(typet &type)
{
  // they all have a width
  exprt size_expr=
    static_cast<const exprt &>(type.find(ID_size));

  typecheck_expr(size_expr);
  make_constant_index(size_expr);

  mp_integer size_int;
  if(to_integer(size_expr, size_int))
  {
    err_location(size_expr);
    throw "failed to convert bit vector width to constant";
  }

  if(size_int<1 || size_int>1024)
  {
    err_location(size_expr);
    error("bit vector width invalid");
    throw 0;
  }
  
  type.remove(ID_size);
  type.set(ID_width, integer2string(size_int));

  // depending on type, there may be a number of fractional bits
  
  if(type.id()==ID_custom_unsignedbv)
    type.id(ID_unsignedbv);  
  else if(type.id()==ID_custom_signedbv)
    type.id(ID_signedbv);  
  else if(type.id()==ID_custom_fixedbv)
  {
    type.id(ID_fixedbv);  

    exprt f_expr=
      static_cast<const exprt &>(type.find(ID_f));

    locationt location=f_expr.find_location();

    typecheck_expr(f_expr);
    
    make_constant_index(f_expr);

    mp_integer f_int;
    if(to_integer(f_expr, f_int))
    {
      err_location(location);
      throw "failed to convert number of fraction bits to constant";
    }

    if(f_int<0 || f_int>size_int)
    {
      err_location(location);
      error("fixedbv fraction width invalid");
      throw 0;
    }
    
    type.remove(ID_f);
    type.set(ID_integer_bits, integer2string(size_int-f_int));
  }
  else if(type.id()==ID_custom_floatbv)
  {
    type.id(ID_floatbv);  

    exprt f_expr=
      static_cast<const exprt &>(type.find(ID_f));
      
    locationt location=f_expr.find_location();

    typecheck_expr(f_expr);
    
    make_constant_index(f_expr);

    mp_integer f_int;
    if(to_integer(f_expr, f_int))
    {
      err_location(location);
      throw "failed to convert number of fraction bits to constant";
    }

    if(f_int<1 || f_int+1>=size_int)
    {
      err_location(location);
      error("floatbv fraction width invalid");
      throw 0;
    }
    
    type.remove(ID_f);
    type.set(ID_f, integer2string(f_int));
  }
  else
    assert(false);
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_code_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_code_type(code_typet &type)
{
  // the return type is still 'subtype()'
  type.return_type()=type.subtype();
  type.remove(ID_subtype);

  code_typet::parameterst &parameters=type.parameters();
  
  // if we don't have any parameters, we assume it's (...)
  if(parameters.empty())
  {
    type.make_ellipsis();
  }
  else // we do have parameters
  {
    // is the last one ellipsis?
    if(type.parameters().back().id()==ID_ellipsis)
    {
      type.make_ellipsis();
      type.parameters().pop_back();
    }
    
    parameter_map.clear();
  
    for(code_typet::parameterst::iterator
        p_it=type.parameters().begin();
        p_it!=type.parameters().end();
        p_it++)
    {
      // turn the declarations into parameters
      if(p_it->id()==ID_declaration)
      {
        ansi_c_declarationt &declaration=to_ansi_c_declaration(*p_it);
      
        code_typet::parametert parameter;

        // first fix type
        typet &type=parameter.type();
        type=declaration.full_type(declaration.declarator());
        std::list<codet> tmp_clean_code;
        tmp_clean_code.swap(clean_code); // ignore side-effects
        typecheck_type(type);
        tmp_clean_code.swap(clean_code);
        adjust_function_parameter(type);
      
        // adjust the identifier
        irep_idt identifier=declaration.declarator().get_name();

        // abstract or not?
        if(identifier==irep_idt())
        {
          // abstract
          parameter.location()=declaration.type().location();
        }
        else
        {
          identifier=add_language_prefix(identifier);
    
          // make visible now, later parameters might use it
          parameter_map[identifier]=type;
          parameter.set_base_name(declaration.declarator().get_base_name());
          parameter.location()=declaration.declarator().location();
        }
        
        // put the parameter in place of the declaration
        p_it->swap(parameter);
      }
    }
    
    parameter_map.clear();
  
    if(parameters.size()==1 &&
       follow(parameters[0].type()).id()==ID_empty)
    {
      // if we just have one parameter of type void, remove it
      parameters.clear();
    }
  }

  typecheck_type(type.return_type());
  
  // 6.7.6.3:
  // "A function declarator shall not specify a return type that
  // is a function type or an array type."
  
  const typet &return_type=follow(type.return_type());
  
  if(return_type.id()==ID_array)
  {
    err_location(type);
    throw "function must not return array";
  }
  
  if(return_type.id()==ID_code)
  {
    err_location(type);
    throw "function must not return function type";
  }
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_array_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_array_type(array_typet &type)
{
  exprt &size=type.size();
  locationt location=size.find_location();

  // check subtype
  typecheck_type(type.subtype());
  
  // we don't allow void as subtype
  if(follow(type.subtype()).id()==ID_empty)
  {
    err_location(type);
    str << "array of voids";
    throw 0;
  }

  // check size, if any
  
  if(size.is_not_nil())
  {
    typecheck_expr(size);
    make_index_type(size);
    
    // The size need not be a constant!
    // We simplify it, for the benefit of array initialisation.
    
    exprt tmp_size=size;
    simplify(tmp_size, *this);

    if(tmp_size.is_constant())
    {
      mp_integer s;
      if(to_integer(tmp_size, s))
      {
        err_location(location);
        str << "failed to convert constant: "
            << tmp_size.pretty();
        throw 0;
      }

      if(s<0)
      {
        err_location(location);
        str << "array size must not be negative, "
               "but got " << s;
        throw 0;
      }
      
      size=tmp_size;
    }
    else if(tmp_size.id()==ID_infinity)
    {
      size=tmp_size;
    }
    else if(tmp_size.id()==ID_symbol &&
            tmp_size.type().get_bool(ID_C_constant))
    {
      // We allow a constant variable as array size, assuming
      // it won't change.
      // This criterion can be tricked:
      // Of course we can modify a 'const' symbol, e.g.,
      // using a pointer type cast. Interestingly,
      // at least gcc 4.2.1 makes the very same mistake!
      size=tmp_size;
    }
    else
    {
      // not a constant and not infinity
    
      assert(current_symbol_id!=irep_idt());
        
      const symbolt &base_symbol=
        lookup(
          //base_symbol_identifier!=irep_idt()?
          //base_symbol_identifier:
          current_symbol_id);
      
      // Need to pull out! We insert new symbol.
      locationt location=size.find_location();
      unsigned count=0;
      irep_idt temp_identifier;
      std::string suffix;

      do
      {
        suffix="$array_size"+i2string(count);
        temp_identifier=id2string(base_symbol.name)+suffix;
        count++;
      }
      while(symbol_table.symbols.find(temp_identifier)!=symbol_table.symbols.end());

      // add the symbol to symbol table
      symbolt new_symbol;
      new_symbol.name=temp_identifier;
      new_symbol.pretty_name=id2string(base_symbol.pretty_name)+suffix;
      new_symbol.base_name=id2string(base_symbol.base_name)+suffix;
      new_symbol.type=size.type();
      new_symbol.type.set(ID_C_constant, true);
      new_symbol.is_file_local=true;
      new_symbol.is_type=false;
      new_symbol.is_thread_local=true;
      new_symbol.is_static_lifetime=false;
      new_symbol.value.make_nil();
      new_symbol.location=location;
      
      symbol_table.add(new_symbol);

      // produce the code that declares and initializes the symbol
      symbol_exprt symbol_expr;
      symbol_expr.set_identifier(temp_identifier);
      symbol_expr.type()=new_symbol.type;
      
      code_declt declaration(symbol_expr);
      declaration.location()=location;

      code_assignt assignment;
      assignment.lhs()=symbol_expr;
      assignment.rhs()=size;
      assignment.location()=location;

      // store the code
      clean_code.push_back(declaration);
      clean_code.push_back(assignment);

      // fix type
      size=symbol_expr;
    }
  }
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_vector_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_vector_type(vector_typet &type)
{
  exprt &size=type.size();
  locationt size_location=size.find_location();

  typecheck_expr(size);
  
  typet &subtype=type.subtype();
  typecheck_type(subtype);

  // we are willing to combine 'vector' with various
  // other types, but not everything!

  if(subtype.id()!=ID_signedbv &&
     subtype.id()!=ID_unsignedbv &&
     subtype.id()!=ID_floatbv &&
     subtype.id()!=ID_fixedbv)
  {
    err_location(size_location);
    str << "cannot make a vector of subtype "
        << to_string(subtype);
    throw 0;
  }

  make_constant_index(size);

  mp_integer s;
  if(to_integer(size, s))
  {
    err_location(size_location);
    str << "failed to convert constant: "
        << size.pretty();
    throw 0;
  }

  if(s<=0)
  {
    err_location(size_location);
    str << "vector size must be positive, "
           "but got " << s;
    throw 0;
  }
  
  // the subtype must have constant size
  exprt size_expr=c_sizeof(type.subtype(), *this);

  simplify(size_expr, *this);

  mp_integer sub_size;
  
  if(to_integer(size_expr, sub_size))
  {
    err_location(size_location);
    str << "failed to determine size of vector base type `"
        << to_string(type.subtype()) << "'";
    throw 0;
  }

  if(sub_size==0)
  {
    err_location(size_location);
    str << "type had size 0: `"
        << to_string(type.subtype()) << "'";
    throw 0;
  }
  
  // adjust by width of base type
  if(s%sub_size!=0)
  {
    err_location(size_location);
    str << "vector size (" << s
        << ") expected to be multiple of base type size (" << sub_size
        << ")";
    throw 0;
  }
  
  s/=sub_size;

  type.set(ID_size, from_integer(s, signed_size_type()));
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_compound_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_compound_type(struct_union_typet &type)
{
  // These get replaced by symbol types.
  irep_idt identifier;
  
  bool have_body=type.find(ID_components).is_not_nil();
  
  if(type.find(ID_tag).is_nil())
  {
    // Anonymous? Must come with body.
    assert(have_body);

    // produce symbol
    symbolt compound_symbol;
    compound_symbol.is_type=true;
    compound_symbol.type=type;
    compound_symbol.location=type.location();

    typecheck_compound_body(compound_symbol);

    std::string typestr=type2name(compound_symbol.type);
    compound_symbol.base_name="#anon-"+typestr;
    compound_symbol.name=add_language_prefix("tag-#anon#"+typestr);
    identifier=compound_symbol.name;
    
    // We might already have the same anonymous union/struct,
    // and this is simply ok. Note that the C standard treats
    // these as different types.
    if(symbol_table.symbols.find(identifier)==symbol_table.symbols.end())
    {
      symbolt *new_symbol;
      move_symbol(compound_symbol, new_symbol);
    }
  }
  else
  {
    identifier=add_language_prefix(type.find(ID_tag).get(ID_identifier));
    
    // does it exist already?
    symbol_tablet::symbolst::iterator s_it=
      symbol_table.symbols.find(identifier);

    if(s_it==symbol_table.symbols.end())
    {
      // no, add new symbol
      irep_idt base_name=type.find(ID_tag).get(ID_C_base_name);
      type.remove(ID_tag);
      type.set(ID_tag, base_name);

      symbolt compound_symbol;
      compound_symbol.is_type=true;
      compound_symbol.name=identifier;
      compound_symbol.base_name=base_name;
      compound_symbol.type=type;
      compound_symbol.location=type.location();
      compound_symbol.pretty_name=id2string(type.id())+" "+id2string(base_name);

      if(have_body)
      {
        // add before doing the body (may be recursive)      
        symbolt *new_symbol;
        move_symbol(compound_symbol, new_symbol);

        typecheck_compound_body(*new_symbol);
      }
      else
      {
        if(compound_symbol.type.id()==ID_struct)
          compound_symbol.type.id(ID_incomplete_struct);
        else if(compound_symbol.type.id()==ID_union)
          compound_symbol.type.id(ID_incomplete_union);
        else
          assert(false);

        symbolt *new_symbol;
        move_symbol(compound_symbol, new_symbol);
      }
    }
    else
    {
      // yes, it exists already
      if(s_it->second.type.id()==ID_incomplete_struct ||
         s_it->second.type.id()==ID_incomplete_union)
      {
        // Maybe we got a body now.
        if(have_body)
        {
          irep_idt base_name=type.find(ID_tag).get(ID_C_base_name);
          type.remove(ID_tag);
          type.set(ID_tag, base_name);

          s_it->second.type=type;
          typecheck_compound_body(s_it->second);
        }
      }
      else if(have_body)
      {
        err_location(type);
        str << "redefinition of body of `" << s_it->second.pretty_name << "'";
        error();
        throw 0;
      }
    }
  }

  symbol_typet symbol_type;
  symbol_type.location()=type.location();
  symbol_type.set_identifier(identifier);

  type.swap(symbol_type);
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_compound_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_compound_body(symbolt &symbol)
{
  struct_union_typet::componentst &components=
    to_struct_union_type(symbol.type).components();

  struct_union_typet::componentst old_components;
  old_components.swap(components);

  // We get these as declarations!
  for(struct_union_typet::componentst::iterator
      it=old_components.begin();
      it!=old_components.end();
      it++)
  {
    // the arguments are member declarations or static assertions
    assert(it->id()==ID_declaration);
    
    ansi_c_declarationt &declaration=
      to_ansi_c_declaration(static_cast<exprt &>(*it));
      
    if(declaration.get_is_static_assert())
    {
      struct_union_typet::componentt new_component;
      new_component.id(ID_static_assert);
      new_component.location()=declaration.location();
      new_component.operands().swap(declaration.operands());
      assert(new_component.operands().size()==2);
      components.push_back(new_component);
    }
    else
    {
      // do first half of type
      typecheck_type(declaration.type());
      make_already_typechecked(declaration.type());
    
      for(ansi_c_declarationt::declaratorst::iterator
          d_it=declaration.declarators().begin();
          d_it!=declaration.declarators().end();
          d_it++)
      {
        struct_union_typet::componentt new_component;

        new_component.location()=d_it->location();
        new_component.set(ID_name, d_it->get_base_name());
        new_component.set(ID_pretty_name, d_it->get_base_name());
        new_component.type()=declaration.full_type(*d_it);

        // mark bit-fields as such
        if(new_component.type().id()==ID_c_bitfield)
        {
          new_component.set_is_bit_field(true);
          typet tmp=new_component.type().subtype();
          typecheck_type(tmp);
          new_component.set_bit_field_type(tmp);
        }
        
        typecheck_type(new_component.type());

        components.push_back(new_component);
      }
    }
  }

  unsigned anon_member_counter=0;

  // scan for anonymous members, and name them
  for(struct_union_typet::componentst::iterator
      it=components.begin();
      it!=components.end();
      it++)
  {
    if(it->get_name()!=irep_idt()) continue;

    it->set_name("$anon"+i2string(anon_member_counter++));
    it->set_anonymous(true);
  }

  // scan for duplicate members

  {
    hash_set_cont<irep_idt, irep_id_hash> members;

    for(struct_union_typet::componentst::iterator
        it=components.begin();
        it!=components.end();
        it++)
    {
      if(!members.insert(it->get_name()).second)
      {
        // we do nothing (as gcc won't complain)
      }
    }
  }
  
  // We allow an incomplete (C99) array as _last_ member!
  // Zero-length is allowed everywhere.

  if(symbol.type.id()==ID_struct)
  {
    for(struct_union_typet::componentst::iterator
        it=components.begin();
        it!=components.end();
        it++)
    {
      typet &type=it->type();
    
      if(type.id()==ID_array &&
         to_array_type(type).is_incomplete())
      {
        // needs to be last member
        if(it!=--components.end())
        {
          err_location(*it);
          throw "flexible struct member must be last member";
        }
        
        // make it zero-length
        type.id(ID_array);
        type.set(ID_size, gen_zero(index_type()));
      }
    }  
  }

  // we may add some minimal padding inside structs (not unions)
  // unless there is an attribute that says that the struct is
  // 'packed'

  if(symbol.type.id()==ID_struct)
    add_padding(to_struct_type(symbol.type), *this);

  // now remove zero-width bit-fields
  for(struct_typet::componentst::iterator
      it=components.begin();
      it!=components.end();
      ) // blank
  {
    if(it->get_is_bit_field() &&
       it->type().get_int(ID_width)==0)
      it=components.erase(it);
    else
      it++;
  }

  // finally, check _Static_assert inside the compound
  for(struct_union_typet::componentst::iterator
      it=components.begin();
      it!=components.end();
      ) // no it++
  {
    if(it->id()==ID_static_assert)
    {
      assert(it->operands().size()==2);
      exprt &assertion=it->op0();
      typecheck_expr(assertion);
      typecheck_expr(it->op1());
      assertion.make_typecast(bool_typet());
      make_constant(assertion);
      
      if(assertion.is_false())
      {
        err_location(*it);
        throw "failed _Static_assert";
      }
      else if(!assertion.is_true())
      {
        // should warn/complain
      }
      
      it=components.erase(it);
    }
    else
      it++;
  }  
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_c_enum_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_c_enum_type(typet &type)
{
  // these have the declarations of the enum constants as operands
  exprt &as_expr=static_cast<exprt &>(static_cast<irept &>(type));
  locationt location=type.location();

  // enums start at zero
  mp_integer value=0;
  
  // track min and max to find a nice base type
  mp_integer min_value=0, max_value=0;
  
  Forall_operands(it, as_expr)
  {
    ansi_c_declarationt &declaration=to_ansi_c_declaration(*it);
    
    // In C, enum constants always have type "int". They do not
    // have the enum type.
    declaration.type()=typet(ID_int);

    exprt &v=declaration.declarator().value();

    if(v.is_nil()) // no value given
      v=from_integer(value, int_type());

    typecheck_declaration(declaration);
    
    // produce value for next constant
    const symbolt &symbol=
      lookup(language_prefix+id2string(declaration.declarator().get_name()));
    to_integer(symbol.value, value);
    
    if(value<min_value) min_value=value;
    if(value>max_value) max_value=value;
    
    ++value;
  }
  
  // remove these now
  as_expr.operands().clear();

  // We need to determine a width, and a signedness.
  // We just do int, but gcc might pick smaller widths
  // if the type is marked as 'packed'.

  unsigned bits=0;
  bool is_signed=min_value<0;
  
  if(is_signed)
  {
    if(max_value<(1<<7) && min_value>=-(1<<7))
      bits=1*8;
    else if(max_value<(1<<15) && min_value>=-(1<<15))
      bits=2*8;
    else if(max_value<(mp_integer(1)<<31) && min_value>=-(mp_integer(1)<<31))
      bits=4*8;
    else
      bits=8*8;
  }
  else // unsigned
  {
    if(max_value<(1<<8))
      bits=1*8;
    else if(max_value<(1<<16))
      bits=2*8;
    else if(max_value<(mp_integer(1)<<32))
      bits=4*8;
    else
      bits=8*8;
  }

  if(!type.get_bool(ID_C_packed))
  {
    // we do int as a minimum
    if(bits<config.ansi_c.int_width)
      bits=config.ansi_c.int_width;
  }

  type.set(ID_width, bits);
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_c_bit_field_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_c_bit_field_type(typet &type)
{
  typecheck_type(type.subtype());

  exprt &width_expr=static_cast<exprt &>(type.add(ID_size));

  typecheck_expr(width_expr);
  make_constant_index(width_expr);

  mp_integer i;
  if(to_integer(width_expr, i))
  {
    err_location(type);
    throw "failed to convert bit field width";
  }

  if(i<0)
  {
    err_location(type);
    throw "bit field width is negative";
  }

  const typet &base_type=follow(type.subtype());
  
  if(base_type.id()==ID_bool)
  {
    if(i>1)
    {
      err_location(type);
      throw "bit field width too large";
    }

    // We don't use bool, as it's really a byte long.
    type.id(ID_unsignedbv);
    type.set(ID_width, integer2long(i));
  }
  else if(base_type.id()==ID_signedbv ||
          base_type.id()==ID_unsignedbv ||
          base_type.id()==ID_c_enum)
  {
    unsigned width=base_type.get_int(ID_width);

    if(i>width)
    {
      err_location(type);
      throw "bit field width too large";
    }

    typet tmp(base_type);
    type.swap(tmp);
    type.set(ID_width, integer2string(i));
  }
  else
  {
    err_location(type);
    str << "bit field with non-integer type: "
        << to_string(base_type);
    throw 0;
  }
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_typeof_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_typeof_type(typet &type)
{
  // save location
  locationt location=type.location();

  // retain the qualifiers as is
  c_qualifierst c_qualifiers;
  c_qualifiers.read(type);

  if(!((const exprt &)type).has_operands())
  {
    typet t=static_cast<const typet &>(type.find(ID_type_arg));
    typecheck_type(t);
    type.swap(t);
  }
  else
  {
    exprt expr=((const exprt &)type).op0();
    typecheck_expr(expr);

    // undo an implicit address-of
    if(expr.id()==ID_address_of &&
       expr.get_bool(ID_C_implicit))
    {
      assert(expr.operands().size()==1);
      exprt tmp;
      tmp.swap(expr.op0());
      expr.swap(tmp);
    }

    type.swap(expr.type());
  }
  
  type.location()=location;
  c_qualifiers.write(type);
}

/*******************************************************************\

Function: c_typecheck_baset::typecheck_symbol_type

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::typecheck_symbol_type(typet &type)
{
  {
    // add prefix
    symbol_typet &symbol_type=to_symbol_type(type);
    symbol_type.set_identifier(add_language_prefix(symbol_type.get_identifier()));
  }

  const irep_idt &identifier=
    to_symbol_type(type).get_identifier();

  symbol_tablet::symbolst::const_iterator s_it=
    symbol_table.symbols.find(identifier);

  if(s_it==symbol_table.symbols.end())
  {
    err_location(type);
    str << "type symbol `" << identifier << "' not found";
    throw 0;
  }

  const symbolt &symbol=s_it->second;

  if(!symbol.is_type)
  {
    err_location(type);
    throw "expected type symbol";
  }
  
  if(symbol.is_macro)
  {
    // overwrite, but preserve (add) any qualifiers and other flags

    c_qualifierst c_qualifiers(type);
    bool is_packed=type.get_bool(ID_C_packed);
    irept alignment=type.find(ID_C_alignment);
    
    c_qualifiers+=c_qualifierst(symbol.type);
    type=symbol.type;
    c_qualifiers.write(type);
    
    if(is_packed) type.set(ID_C_packed, true);
    if(alignment.is_not_nil()) type.set(ID_C_alignment, alignment);
  }
    
  // CPROVER extensions
  if(symbol.base_name=="__CPROVER_rational")
  {
    type=rational_typet();
  }
  else if(symbol.base_name=="__CPROVER_integer")
  {
    type=integer_typet();
  }
}

/*******************************************************************\

Function: c_typecheck_baset::adjust_function_parameter

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void c_typecheck_baset::adjust_function_parameter(typet &type) const
{
  if(type.id()==ID_array)
  {
    type.id(ID_pointer);
    type.remove(ID_size);
    type.remove(ID_C_constant);
  }
  else if(type.id()==ID_code)
  {
    // see ISO/IEC 9899:1999 page 199 clause 8,
    // may be hidden in typedef
    pointer_typet tmp;
    tmp.subtype()=type;
    type.swap(tmp);
  }
  else if(type.id()==ID_KnR)
  {
    // any KnR args without type yet?
    type=signed_int_type(); // the default is integer!
  }
}

