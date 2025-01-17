/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "boolbv.h"

#include <util/invariant.h>

bvt boolbvt::convert_cond(const cond_exprt &expr)
{
  const exprt::operandst &operands=expr.operands();

  std::size_t width=boolbv_width(expr.type());

  if(width==0)
    return conversion_failed(expr);

  bvt bv;

  DATA_INVARIANT(operands.size() >= 2, "cond must have at least two operands");

  DATA_INVARIANT(
    operands.size() % 2 == 0, "number of cond operands must be even");

  if(prop.has_set_to())
  {
    bool condition=true;
    literalt previous_cond=const_literal(false);
    literalt cond_literal=const_literal(false);

    // make it free variables
    bv = prop.new_variables(width);

    forall_operands(it, expr)
    {
      if(condition)
      {
        cond_literal=convert(*it);
        cond_literal=prop.land(!previous_cond, cond_literal);
        prop.push_control_dep(cond_literal);

        previous_cond=prop.lor(previous_cond, cond_literal);
      }
      else
      {
        const bvt &op = convert_bv(*it, bv.size());

        literalt value_literal=bv_utils.equal(bv, op);

        prop.l_set_to_true(prop.limplies(cond_literal, value_literal));
      }

      condition=!condition;
    }
    for (size_t i = 0; i < expr.operands().size(); i += 2)
    {
      prop.pop_control_dep();
    }
  }
  else
  {
    bv.resize(width);

    // functional version -- go backwards
    for(std::size_t i=expr.operands().size(); i!=0; i-=2)
    {
      INVARIANT(
        i >= 2,
        "since the number of operands is even if i is nonzero it must be "
        "greater than two");
      const exprt &cond=expr.operands()[i-2];
      const exprt &value=expr.operands()[i-1];

      literalt cond_literal=convert(cond);
      prop.push_control_dep(cond_literal);
      const bvt &op = convert_bv(value, bv.size());

      for(std::size_t j = 0; j < bv.size(); j++)
        bv[j] = prop.lselect(cond_literal, op[j], bv[j]);
      prop.pop_control_dep();
    }
  }

  return bv;
}
