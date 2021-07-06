/*******************************************************************\

Module: CNF Generation

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

/// \file
/// CNF Generation

#include "cnf_clause_list.h"

#include <iostream>
#include <ostream>

void cnf_clause_listt::relationless_lcnf(const bvt &bv)
{
  bvt new_bv;

  if(process_clause(bv, new_bv))
    return;

  clauses.push_back(new_bv);
}

void cnf_clause_list_assignmentt::print_assignment(std::ostream &out) const
{
  for(unsigned v=1; v<assignment.size(); v++)
    out << "v" << v << "=" << assignment[v] << "\n";
}

void cnf_clause_list_assignmentt::copy_assignment_from(const propt &prop)
{
  assignment.resize(no_variables());

  // we don't use index 0, start with 1
  for(unsigned v=1; v<assignment.size(); v++)
  {
    literalt l;
    l.set(v, false);
    assignment[v]=prop.l_get(l);
  }
}

void relationst::relate(literalt from, literalt to)
{
  if(!from.is_constant() && !to.is_constant())
  {
    if(relations.size() <= to.var_no())
    {
      relations.resize(to.var_no() + 1);
    }
    relations.at(to.var_no()).push_back(from.var_no());
  }
}

void relationst::write_relations(std::ostream &out)
{
  for(size_t i = 0; i < relations.size(); i++)
  {
    auto &froms = relations.at(i);
    if(!froms.empty())
    {
      out << "c __rel__ " << i;
      for(const auto &from : froms)
      {
        out << " " << from;
      }
      out << "\n";
    }
  }
}