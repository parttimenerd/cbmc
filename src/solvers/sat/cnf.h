/*******************************************************************\

Module: CNF Generation, via Tseitin

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

/// \file
/// CNF Generation, via Tseitin

#ifndef CPROVER_SOLVERS_SAT_CNF_H
#define CPROVER_SOLVERS_SAT_CNF_H

#include <solvers/prop/prop.h>

class cnft:public propt
{
public:
  // For CNF, we don't use index 0 as a matter of principle,
  // so we'll start counting variables at 1.
  explicit cnft(message_handlert &message_handler)
    : cnft(message_handler, false)
  {
  }
  cnft(message_handlert &message_handler, bool supports_relations)
    : propt(message_handler),
      _no_variables(1),
      supports_relations(supports_relations)
  {
  }
  virtual ~cnft() { }

  virtual literalt land(literalt a, literalt b) override;
  virtual literalt lor(literalt a, literalt b) override;
  virtual literalt land(const bvt &bv) override;
  virtual literalt lor(const bvt &bv) override;
  virtual literalt lxor(const bvt &bv) override;
  virtual literalt lxor(literalt a, literalt b) override;
  virtual literalt lnand(literalt a, literalt b) override;
  virtual literalt lnor(literalt a, literalt b) override;
  virtual literalt lequal(literalt a, literalt b) override;
  virtual literalt limplies(literalt a, literalt b) override;
  // a?b:c
  virtual literalt lselect(literalt a, literalt b, literalt c) override;
  virtual literalt new_variable() override;
  bvt new_variables(std::size_t width) override;
  virtual size_t no_variables() const override { return _no_variables; }
  virtual void set_no_variables(size_t no) { _no_variables=no; }
  virtual size_t no_clauses() const=0;
  // create a relation from the "from" variable to the "to" variable
  virtual void relate(literalt from, literalt to)
  {
    assert(!supports_relations);
  }
  virtual void lcnf(const bvt &bv) override;

  literalt wrap(const literalt &literal) override;

protected:
  void gate_and(literalt a, literalt b, literalt o);
  void gate_or(literalt a, literalt b, literalt o);
  void gate_xor(literalt a, literalt b, literalt o);
  void gate_nand(literalt a, literalt b, literalt o);
  void gate_nor(literalt a, literalt b, literalt o);
  void gate_equal(literalt a, literalt b, literalt o);
  void gate_implies(literalt a, literalt b, literalt o);
  virtual void relationless_lcnf(const bvt &bv)
  {
    assert(!supports_relations);
    lcnf(bv);
  }
  void relationless_lcnf(literalt l0, literalt l1, literalt l2)
  {
    lcnf_bv.resize(3);
    lcnf_bv[0] = l0;
    lcnf_bv[1] = l1;
    lcnf_bv[2] = l2;
    relationless_lcnf(lcnf_bv);
  }

  static bvt eliminate_duplicates(const bvt &);

  size_t _no_variables;

  bool supports_relations;

  bool process_clause(const bvt &bv, bvt &dest) const;

  static bool is_all(const bvt &bv, literalt l)
  {
    for(const auto &literal : bv)
    {
      if(literal != l)
        return false;
    }
    return true;
  }
};

class cnf_solvert:public cnft
{
public:
  explicit cnf_solvert(message_handlert &message_handler)
    : cnft(message_handler), status(statust::INIT), clause_counter(0)
  {
  }

  virtual size_t no_clauses() const override
  {
    return clause_counter;
  }

protected:
  enum class statust { INIT, SAT, UNSAT, ERROR };
  statust status;
  size_t clause_counter;
};

#endif // CPROVER_SOLVERS_SAT_CNF_H
