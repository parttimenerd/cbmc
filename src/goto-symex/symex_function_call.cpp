/*******************************************************************\

Module: Symbolic Execution of ANSI-C

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

/// \file
/// Symbolic Execution of ANSI-C

#include "goto_symex.h"

#include <util/arith_tools.h>
#include <util/byte_operators.h>
#include <util/c_types.h>
#include <util/exception_utils.h>
#include <util/fresh_symbol.h>
#include <util/invariant.h>
#include <util/prefix.h>
#include <util/range.h>

#include "expr_skeleton.h"
#include "goto-programs/remove_returns.h"
#include "path_storage.h"
#include "symex_assign.h"

static void locality(
  const irep_idt &function_identifier,
  goto_symext::statet &state,
  path_storaget &path_storage,
  const goto_functionst::goto_functiont &goto_function,
  const namespacet &ns);

recursing_decisiont
goto_symext::get_unwind_recursion(const irep_idt &, unsigned, unsigned)
{
  return recursing_decisiont::RESUME;
}

void goto_symext::parameter_assignments(
  const irep_idt &function_identifier,
  const goto_functionst::goto_functiont &goto_function,
  statet &state,
  const exprt::operandst &arguments,
  const std::function<void(dstringt)> &argument_call_back)
{
  // iterates over the arguments
  exprt::operandst::const_iterator it1=arguments.begin();

  // iterates over the types of the parameters
  for(const auto &identifier : goto_function.parameter_identifiers)
  {
    INVARIANT(
      !identifier.empty(), "function parameter must have an identifier");
    state.call_stack().top().parameter_names.push_back(identifier);

    const symbolt &symbol=ns.lookup(identifier);
    symbol_exprt lhs=symbol.symbol_expr();

    // this is the type that the n-th argument should have
    const typet &parameter_type = symbol.type;

    exprt rhs;

    // if you run out of actual arguments there was a mismatch
    if(it1==arguments.end())
    {
      log.warning() << state.source.pc->source_location.as_string()
                    << ": "
                       "call to '"
                    << id2string(function_identifier)
                    << "': "
                       "not enough arguments, inserting non-deterministic value"
                    << log.eom;

      rhs = side_effect_expr_nondett(
        parameter_type, state.source.pc->source_location);
    }
    else
      rhs=*it1;

    if(rhs.is_nil())
    {
      // 'nil' argument doesn't get assigned
    }
    else
    {
      // It should be the same exact type.
      if(parameter_type != rhs.type())
      {
        const typet &rhs_type = rhs.type();

        // But we are willing to do some limited conversion.
        // This is highly dubious, obviously.
        // clang-format off
        if(
          (parameter_type.id() == ID_signedbv ||
           parameter_type.id() == ID_unsignedbv ||
           parameter_type.id() == ID_c_enum_tag ||
           parameter_type.id() == ID_bool ||
           parameter_type.id() == ID_pointer ||
           parameter_type.id() == ID_union ||
           parameter_type.id() == ID_union_tag) &&
          (rhs_type.id() == ID_signedbv ||
           rhs_type.id() == ID_unsignedbv ||
           rhs_type.id() == ID_c_bit_field ||
           rhs_type.id() == ID_c_enum_tag ||
           rhs_type.id() == ID_bool ||
           rhs_type.id() == ID_pointer ||
           rhs_type.id() == ID_union ||
           rhs_type.id() == ID_union_tag))
        // clang-format on
        {
          rhs = make_byte_extract(
            rhs, from_integer(0, index_type()), parameter_type);
        }
        else
        {
          std::ostringstream error;
          error << state.source.pc->source_location.as_string() << ": "
                << "function call: parameter \"" << identifier
                << "\" type mismatch:\ngot " << rhs.type().pretty()
                << "\nexpected " << parameter_type.pretty();
          throw unsupported_operation_exceptiont(error.str());
        }
      }

      assignment_typet assignment_type;

      // We hide if we are in a hidden function.
      if(state.call_stack().top().hidden_function)
        assignment_type =
          symex_targett::assignment_typet::HIDDEN_ACTUAL_PARAMETER;
      else
        assignment_type =
          symex_targett::assignment_typet::VISIBLE_ACTUAL_PARAMETER;

      lhs = to_symbol_expr(clean_expr(std::move(lhs), state, true));
      rhs = clean_expr(std::move(rhs), state, false);
      //std::cerr << "                        " << "assign parameter " << lhs.to_string2() << " = " << rhs.to_string2() << "\n";
      exprt::operandst lhs_conditions;
      symex_assignt{state, assignment_type, ns, symex_config, target}
        .assign_rec(lhs, expr_skeletont{}, rhs, lhs_conditions);
      auto renamed = state.rename(symbol.symbol_expr(), ns).get();
      if(!renamed.is_constant())
      {
        argument_call_back(lhs.get_identifier());
      }
    }

    if(it1!=arguments.end())
      it1++;
  }

  if(to_code_type(ns.lookup(function_identifier).type).has_ellipsis())
  {
    // These are va_arg arguments; their types may differ from call to call
    for(; it1 != arguments.end(); it1++)
    {
      symbolt &va_arg = get_fresh_aux_symbol(
        it1->type(),
        id2string(function_identifier),
        "va_arg",
        state.source.pc->source_location,
        ns.lookup(function_identifier).mode,
        state.symbol_table);
      va_arg.is_parameter = true;
      argument_call_back(va_arg.name);
      state.call_stack().top().parameter_names.push_back(va_arg.name);

      symex_assign(state, va_arg.symbol_expr(), *it1);
    }
  }
  else if(it1!=arguments.end())
  {
    // we got too many arguments, but we will just ignore them
  }
}

void goto_symext::symex_function_call(
  const get_goto_functiont &get_goto_function,
  statet &state,
  const code_function_callt &code)
{
  const exprt &function=code.function();

  // If at some point symex_function_call can support more
  // expression ids(), like ID_Dereference, please expand the
  // precondition appropriately.
  PRECONDITION(function.id() == ID_symbol);
  symex_function_call_symbol(get_goto_function, state, code);
}

void goto_symext::symex_function_call_symbol(
  const get_goto_functiont &get_goto_function,
  statet &state,
  const code_function_callt &original_code)
{
  code_function_callt code = original_code;

  const irep_idt &identifier = to_symbol_expr(code.function()).get_identifier();

  if(code.lhs().is_not_nil())
    code.lhs() = clean_expr(std::move(code.lhs()), state, true);

  code.function() = clean_expr(std::move(code.function()), state, false);

  for(auto &argument : code.arguments())
    argument = clean_expr(std::move(argument), state, false);

  target.location(state.guard.as_expr(), state.source);

  PRECONDITION(code.function().id() == ID_symbol);

  if(has_prefix(id2string(identifier), CPROVER_FKT_PREFIX))
  {
    symex_fkt(state, code);
  }
  else
    symex_function_call_code(get_goto_function, state, code);
}

dstringt resolve(goto_symex_statet &state, namespacet &ns, dstringt var)
{
  auto renamed = state.rename(ns.lookup(var).symbol_expr(), ns).get();
  return to_symbol_expr(renamed).get_identifier();
}

void assign_unknown(
  goto_symex_statet &state,
  goto_symext &symex,
  namespacet &ns,
  dstringt var)
{
  auto renamed = state.rename(ns.lookup(var).symbol_expr(), ns);
  auto fresh = get_fresh_aux_symbol(
    renamed.get().type(),
    "oa_unknown",
    var.c_str(),
    state.source.pc->source_location,
    ns.lookup(var).mode,
    state.symbol_table);
  auto rhs = symex.clean_expr(fresh.symbol_expr(), state, false);
  exprt::operandst lhs_conditions;
  ssa_exprt new_lhs =
    state.rename_ssa<L1>(ssa_exprt{ns.lookup(var).symbol_expr()}, ns).get();
  auto ret = state.assignment(new_lhs, rhs, ns, true, true);
}

void goto_symext::symex_function_call_code(
  const get_goto_functiont &get_goto_function,
  statet &state,
  const code_function_callt &call)
{
  const irep_idt &identifier=
    to_symbol_expr(call.function()).get_identifier();

  const goto_functionst::goto_functiont &goto_function =
    get_goto_function(identifier);

  path_storage.dirty.populate_dirty_for_function(identifier, goto_function);

  auto emplace_safe_pointers_result =
    path_storage.safe_pointers.emplace(identifier, local_safe_pointerst{});
  if(emplace_safe_pointers_result.second)
    emplace_safe_pointers_result.first->second(goto_function.body);

  auto rec_count = state.call_stack().top().loop_iterations[identifier].count;

  const recursing_decisiont recursing_decision =
    get_unwind_recursion(identifier, state.source.thread_nr, rec_count);

  const auto resolve = [&](dstringt var) { return ::resolve(state, ns, var); };

  const auto assign_unknown = [&](dstringt var) {
    ::assign_unknown(state, *this, ns, var);
  };

  if(getenv("LOG_REC"))
  {
    std::cerr << "------------------ call of function " << identifier
              << "  stop_unwinding = " << recursing_decision
              << " pc = " << state.source.pc->source_location.as_string()
              << " count "
              << state.call_stack().top().loop_iterations[identifier].count
              << "\n";
  }

  // we hide the call if the caller and callee are both hidden
  const bool hidden =
    state.call_stack().top().hidden_function && goto_function.is_hidden();

  // see if it's too much
  if(recursing_decision == recursing_decisiont::ABORT)
  {
    framet &frame = state.call_stack().new_frame(state.source, state.guard);
    // preserve locality of local variables
    locality(identifier, state, path_storage, goto_function, ns);

    frame.end_of_function = --goto_function.body.instructions.end();
    frame.return_value = call.lhs();
    frame.function_identifier = identifier;
    frame.hidden_function = goto_function.is_hidden();

    const framet &p_frame = state.call_stack().previous_frame();
    for(const auto &pair : p_frame.loop_iterations)
    {
      if(pair.second.is_recursion)
        frame.loop_iterations.insert(pair);
    }

    // increase unwinding counter
    frame.loop_iterations[identifier].is_recursion = true;
    frame.loop_iterations[identifier].count++;

    // we can shortcut this if we are in the aborted recursion
    // … as the API is far better as the old one
    // … maybe rewrite it in the future
    if(ls_stack.abstract_recursion().enabled())
    {
      parameter_assignments(
        identifier, goto_function, state, call.arguments(), [](dstringt arg) {
        });
      ls_stack.abstract_recursion().create_rec_child(
        identifier, state.guard, resolve, assign_unknown);
      target.function_return(
        state.guard.as_expr(), identifier, state.source, hidden);

      if(call.lhs().is_not_nil())
      {
        const auto rhs =
          side_effect_expr_nondett(call.lhs().type(), call.source_location());
        code_assignt code(call.lhs(), rhs);
        symex_assign(state, code);
      }
      symex_transition(state);
      return;
    }

    ls_stack.push_aborted_recursion(identifier.c_str(), state, resolve);

    // Only enable loop analysis when complexity is enabled.
    if(symex_config.complexity_limits_active)
    {
      // Analyzes loops if required.
      path_storage.add_function_loops(identifier, goto_function.body);
      //frame.loops_info = path_storage.get_loop_analysis(identifier);
    }

    auto argument_call_back = [&](dstringt argument) {
      ls_stack.current_aborted_recursion()->assign_parameter(argument);
    };

    // assign actuals to formal parameters
    parameter_assignments(
      identifier, goto_function, state, call.arguments(), argument_call_back);

    if(getenv("LOG_REC"))
    {
      std::cerr << "abort recursion of " << identifier.c_str() << "\n";
    }

    // push the aborted recursion and register the read globals

    //symex_assume_l2(state, false_exprt()); // this lead to a bug, as it is apparently equivalent to `assume(0)`
  }

  // read the arguments -- before the locality renaming
  const exprt::operandst &arguments = call.arguments();
  const std::vector<renamedt<exprt, L2>> renamed_arguments =
    make_range(arguments).map(
      [&](const exprt &a) { return state.rename(a, ns); });

  if(
    !goto_function.body_available() ||
    recursing_decision == recursing_decisiont::ABORT)
  {
    // record the call
    target.function_call(
      state.guard.as_expr(),
      identifier,
      renamed_arguments,
      state.source,
      hidden);
    if(!goto_function.body_available())
    {
      no_body(identifier);
    }

    // record the return
    target.function_return(
      state.guard.as_expr(), identifier, state.source, hidden);

    if(call.lhs().is_not_nil())
    {
      const auto rhs =
        side_effect_expr_nondett(call.lhs().type(), call.source_location());
      symex_assign(state, call.lhs(), rhs);
    }

    if(symex_config.havoc_undefined_functions)
    {
      // assign non det to function arguments if pointers
      // are not const
      for(const auto &arg : call.arguments())
      {
        if(
          arg.type().id() == ID_pointer &&
          !arg.type().subtype().get_bool(ID_C_constant) &&
          arg.type().subtype().id() != ID_code)
        {
          exprt object = dereference_exprt(arg, arg.type().subtype());
          exprt cleaned_object = clean_expr(object, state, true);
          const guardt guard(true_exprt(), state.guard_manager);
          havoc_rec(state, guard, cleaned_object);
        }
      }
    }
    if(recursing_decision == recursing_decisiont::ABORT)
    {
      ls_stack.current_aborted_recursion()->assign_written_globals(
        [&assign_unknown, &resolve](dstringt var) {
          assign_unknown(var);
          return resolve(var);
        });
    }
    symex_transition(state);
    if(recursing_decision == recursing_decisiont::ABORT)
    {
      ls_stack.pop_aborted_recursion();
    }
    if(getenv("LOG_REC"))
    {
      std::cerr << "end aborted recursion\n";
      std::cerr << "------------------ end aborted recursion of function "
                << identifier << "  stop_unwinding = " << recursing_decision
                << "\n";
    }
    return;
  }

  // produce a new frame
  PRECONDITION(!state.call_stack().empty());

  auto init_frame = [&](bool copy_loop_iterations) -> framet & {
    // record the call
    target.function_call(
      state.guard.as_expr(),
      identifier,
      renamed_arguments,
      state.source,
      hidden);
    framet &frame = state.call_stack().new_frame(state.source, state.guard);

    // Only enable loop analysis when complexity is enabled.
    if(symex_config.complexity_limits_active)
    {
      // Analyzes loops if required.
      path_storage.add_function_loops(identifier, goto_function.body);
      frame.loops_info = path_storage.get_loop_analysis(identifier);
    }

    // preserve locality of local variables
    locality(identifier, state, path_storage, goto_function, ns);

    // assign actuals to formal parameters
    parameter_assignments(identifier, goto_function, state, arguments);

    frame.end_of_function = --goto_function.body.instructions.end();
    frame.return_value = call.lhs();
    frame.function_identifier = identifier;
    frame.hidden_function = goto_function.is_hidden();

    if(copy_loop_iterations)
    {
      const framet &p_frame = state.call_stack().previous_frame();
      for(const auto &pair : p_frame.loop_iterations)
      {
        if(pair.second.is_recursion)
          frame.loop_iterations.insert(pair);
      }
    }

    // increase unwinding counter
    frame.loop_iterations[identifier].is_recursion = true;
    frame.loop_iterations[identifier].count++;

    state.source.function_id = identifier;
    return frame;
  };

  // we handle the case that we have to start an abstract recursion
  if(recursing_decision == recursing_decisiont::FIRST_ABSTRACT_RECURSION)
  {
    PRECONDITION(!ls_stack.abstract_recursion().in_abstract_recursion());

    // maybe check at each end??

    // create new frame…
    auto old_guard = state.guard;
    true_exprt true_expr;
    guard_expr_managert manager; // not used in the implementation
    state.guard = guardt{true_expr, manager};
    framet &frame = init_frame(false);
    frame.end_abstract_recursion = true;

    ls_recursion_node_dbt &rec = ls_stack.abstract_recursion();
    rec.begin_node(
      identifier, old_guard, resolve, assign_unknown, [](const guardt &g) {});

    symex_transition(state, goto_function.body.instructions.begin(), false);

    POSTCONDITION(ls_stack.abstract_recursion().in_abstract_recursion());
  }
  else
  {
    init_frame(true);
    symex_transition(state, goto_function.body.instructions.begin(), false);
    if(getenv("LOG_REC"))
    {
      std::cerr << "------------------ end of function " << identifier
                << "  stop_unwinding = " << recursing_decision << "\n";
    }
  }
}

/// pop one call frame
static void pop_frame(
  goto_symext::statet &state,
  const path_storaget &path_storage,
  bool doing_path_exploration)
{
  PRECONDITION(!state.call_stack().empty());

  const framet &frame = state.call_stack().top();

  // restore program counter
  symex_transition(state, frame.calling_location.pc, false);
  state.source.function_id = frame.calling_location.function_id;

  // restore L1 renaming
  state.level1.restore_from(frame.old_level1);

  // If the program is multi-threaded then the state guard is used to
  // accumulate assumptions (in symex_assume_l2) and must be left alone.
  // If however it is single-threaded then we should restore the guard, as the
  // guard coming out of the function may be more complex (e.g. if the callee
  // was { if(x) while(true) { } } then the guard may still be `!x`),
  // but at this point all control-flow paths have either converged or been
  // proven unviable, so we can stop specifying the callee's constraints when
  // we generate an assumption or VCC.

  // If we're doing path exploration then we do tail-duplication, and we
  // actually *are* in a more-restricted context than we were when the
  // function began.
  if(state.threads.size() == 1 && !doing_path_exploration)
  {
    state.guard = frame.guard_at_function_start;
  }

  for(const irep_idt &l1_o_id : frame.local_objects)
  {
    const auto l2_entry_opt = state.get_level2().current_names.find(l1_o_id);

    if(
      l2_entry_opt.has_value() &&
      (state.threads.size() == 1 ||
       !path_storage.dirty(l2_entry_opt->get().first.get_object_name())))
    {
      state.drop_existing_l1_name(l1_o_id);
    }
  }

  state.call_stack().pop();
}

/// do function call by inlining
void goto_symext::symex_end_of_function(statet &state)
{
  const bool hidden = state.call_stack().top().hidden_function;
  const bool end_abstract_recursion =
    state.call_stack().top().end_abstract_recursion;
  guardt guard = state.guard;
  // first record the return
  target.function_return(
    state.guard.as_expr(), state.source.function_id, state.source, hidden);
  if(end_abstract_recursion)
  {
    ls_stack.abstract_recursion().finish_node(
      state.source.function_id,
      [&](dstringt var) { return ::resolve(state, ns, var); },
      [&](dstringt var) { ::assign_unknown(state, *this, ns, var); },
      [&](guardt g) { guard = g; });
  }

  // then get rid of the frame
  pop_frame(state, path_storage, symex_config.doing_path_exploration);
}

/// Preserves locality of parameters of a given function by applying L1
/// renaming to them.
static void locality(
  const irep_idt &function_identifier,
  goto_symext::statet &state,
  path_storaget &path_storage,
  const goto_functionst::goto_functiont &goto_function,
  const namespacet &ns)
{
  unsigned &frame_nr=
    state.threads[state.source.thread_nr].function_frame[function_identifier];
  frame_nr++;

  for(const auto &param : goto_function.parameter_identifiers)
  {
    (void)state.add_object(
      ns.lookup(param).symbol_expr(),
      [&path_storage, &frame_nr](const irep_idt &l0_name) {
        return path_storage.get_unique_l1_index(l0_name, frame_nr);
      },
      ns);
  }
}
