/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/


#include "boolbv.h"

bvt boolbvt::convert_if(const if_exprt &expr)
{
  std::size_t width=boolbv_width(expr.type());

  if(width==0)
    return bvt(); // An empty bit-vector if.

  literalt cond=convert(expr.cond());

  bv_utils.push_control_dep(cond);

  const bvt &true_case_bv = convert_bv(expr.true_case(), width);
  const bvt &false_case_bv = convert_bv(expr.false_case(), width);

  bv_utils.pop_control_dep();

  return bv_utils.select(cond, true_case_bv, false_case_bv);
}
