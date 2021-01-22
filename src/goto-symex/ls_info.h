//
// Created by bechberger-local on 15.01.21.
//

/// Collected static information (prior to the symbolic execution) used by ls_stack for supporting
/// aborted loops and recursions properly

#ifndef CBMC_LS_INFO_H
#define CBMC_LS_INFO_H
#include <goto-programs/abstract_goto_model.h>

#include <ostream>
#include <utility>

/// infos per functions
class ls_func_info
{
  std::unordered_set<dstringt> assigned_variables;
  /// variables assigned in this function directly
  const std::unordered_set<dstringt> directly_assigned_variables;

  std::unordered_set<dstringt> read_variables;
  /// variables read in this function directly
  const std::unordered_set<dstringt> directly_read_variables;

public:
  const dstringt function_id;
  const goto_functiont &function;
  const std::unordered_set<dstringt> callers;
  const std::unordered_set<dstringt> callees;

  ls_func_info(
    const dstringt function_id,
    const goto_functiont &function,
    std::unordered_set<dstringt> callers,
    std::unordered_set<dstringt> callees,
    std::unordered_set<dstringt> directly_assigned,
    std::unordered_set<dstringt> directly_read)
    : assigned_variables(directly_assigned),
      directly_assigned_variables(std::move(directly_assigned)),
      read_variables(directly_read),
      directly_read_variables(std::move(directly_read)),
      function_id(function_id),
      function(function),
      callers(std::move(callers)),
      callees(std::move(callees))
  {
  }

  /// returns true if one of the variables hasn't been recorded yet
  bool assign_from(const ls_func_info &info);

  const std::unordered_set<dstringt> &get_assigned_variables() const;

  /// contains all variables that are assigned in the bodies of the function and its callees
  const std::unordered_set<dstringt> &get_directly_assigned_variables() const;

  const std::unordered_set<dstringt> &get_read_variables() const;

  /// contains all variables that are assigned in the bodies of the function and its callees
  const std::unordered_set<dstringt> &get_directly_read_variables() const;

  std::unordered_set<dstringt> get_assigned_globals() const;

  std::unordered_set<dstringt> get_read_globals() const;

  friend std::ostream &operator<<(std::ostream &os, const ls_func_info &info);
};

/// precomputed information on (currently) functions
class ls_infot
{
  std::unordered_map<dstringt, ls_func_info> func_infos;

  ls_infot(const std::unordered_map<dstringt, ls_func_info> func_infos)
    : func_infos(func_infos)
  {
  }

public:
  const ls_func_info &get_func_info(dstringt func_name) const;

  static ls_infot create(const goto_functionst &functions);

  const std::unordered_set<dstringt> &
  get_assigned_variables(dstringt func_name) const;

  friend std::ostream &operator<<(std::ostream &os, const ls_infot &info);
};

#endif //CBMC_LS_INFO_H
