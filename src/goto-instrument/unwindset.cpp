/*******************************************************************\

Module: Loop unwinding setup

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "unwindset.h"

#include <util/string2int.h>
#include <util/string_utils.h>

#ifdef _MSC_VER
#  include <util/unicode.h>
#endif

#include <fstream>

void unwindsett::parse_unwind(const std::string &unwind)
{
  if(!unwind.empty())
  {
    global_limit = unsafe_string2unsigned(unwind);
    /*if(global_limit.value() < 3)
    {
      throw "unwind has to be >= 3 or unset";
    }*/
  }
}

void unwindsett::parse_unwindset_one_loop(std::string val)
{
  unsigned thread_nr = 0;
  bool thread_nr_set = false;

  if(!val.empty() && isdigit(val[0]) && val.find(":") != std::string::npos)
  {
    std::string nr = val.substr(0, val.find(":"));
    thread_nr = unsafe_string2unsigned(nr);
    thread_nr_set = true;
    val.erase(0, nr.size() + 1);
  }

  if(val.rfind(":") != std::string::npos)
  {
    std::string id = val.substr(0, val.rfind(":"));
    std::string uw_string = val.substr(val.rfind(":") + 1);

    // the below initialisation makes g++-5 happy
    optionalt<unsigned> uw(0);

    if(uw_string.empty())
      uw = {};
    else
    {
      uw = unsafe_string2unsigned(uw_string);
      /*if(uw.value() < 3)
      {
        throw "unwind has to be >= 3 or unset for all loops";
      }*/
    }
    if(thread_nr_set)
      thread_loop_map[std::pair<irep_idt, unsigned>(id, thread_nr)] = uw;
    else
      loop_map[id] = uw;
  }
}

void unwindsett::parse_unwindset(const std::list<std::string> &unwindset)
{
  for(auto &element : unwindset)
  {
    std::vector<std::string> unwindset_elements =
      split_string(element, ',', true, true);

    for(auto &element : unwindset_elements)
      parse_unwindset_one_loop(element);
  }
}

optionalt<unsigned>
unwindsett::get_limit(const irep_idt &loop_id, unsigned thread_nr) const
{
  // We use the most specific limit we have

  // thread x loop
  auto tl_it =
    thread_loop_map.find(std::pair<irep_idt, unsigned>(loop_id, thread_nr));

  if(tl_it != thread_loop_map.end())
    return tl_it->second;

  // loop
  auto l_it = loop_map.find(loop_id);

  if(l_it != loop_map.end())
    return l_it->second;

  // global, if any
  return global_limit;
}

bool unwindsett::has_specific_limit(const irep_idt &loop_id, unsigned thread_nr)
  const
{
  return thread_loop_map.find(std::pair<irep_idt, unsigned>(
           loop_id, thread_nr)) != thread_loop_map.end() ||
         loop_map.find(loop_id) != loop_map.end();
}

void unwindsett::parse_unwindset_file(const std::string &file_name)
{
  #ifdef _MSC_VER
  std::ifstream file(widen(file_name));
  #else
  std::ifstream file(file_name);
  #endif

  if(!file)
    throw "cannot open file "+file_name;

  std::stringstream buffer;
  buffer << file.rdbuf();

  std::vector<std::string> unwindset_elements =
    split_string(buffer.str(), ',', true, true);

  for(auto &element : unwindset_elements)
    parse_unwindset_one_loop(element);
}
