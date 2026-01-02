////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2006 - 2021, Tomas Babej, Paul Beckingham, Federico Hernandez.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <cmake.h>
// cmake.h include header must come first

#include <CmdModify.h>
#include <Context.h>
#include <Duration.h>
#include <Eval.h>
#include <Filter.h>
#include <Variant.h>
#include <WorkInterval.h>
#include <Datetime.h>
#include <feedback.h>
#include <format.h>
#include <recur.h>
#include <shared.h>
#include <Task.h>

#include <iostream>

#define STRING_CMD_MODIFY_TASK_R "Modifying recurring task {1} '{2}'."
#define STRING_CMD_MODIFY_RECUR \
  "This is a recurring task.  Do you want to modify all pending recurrences of this same task?"

////////////////////////////////////////////////////////////////////////////////
CmdModify::CmdModify() {
  _keyword = "modify";
  _usage = "task <filter> modify <mods>";
  _description = "Modifies the existing task with provided arguments.";
  _read_only = false;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = false;
  _accepts_filter = true;
  _accepts_modifications = true;
  _accepts_miscellaneous = false;
  _category = Command::Category::operation;
}

////////////////////////////////////////////////////////////////////////////////
int CmdModify::execute(std::string&) {
  auto rc = 0;

  // Apply filter.
  Filter filter;
  std::vector<Task> filtered;
  filter.subset(filtered);
  if (filtered.size() == 0) {
    Context::getContext().footnote("No tasks specified.");
    return 1;
  }

  // Accumulated project change notifications.
  std::map<std::string, std::string> projectChanges;

  // Check if we're in interval context (have interval IDs)
  bool in_interval_context = !Context::getContext().cli2._interval_ids.empty();
  
  // If in interval context, handle interval modifications separately
  if (in_interval_context) {
    bool interval_mods_processed = false;
    // Process interval modifications for each task
    for (auto& task : filtered) {
      int task_id = task.id;
      auto it = Context::getContext().cli2._interval_ids.find(task_id);
      if (it != Context::getContext().cli2._interval_ids.end()) {
        int interval_id = it->second;
        std::string task_uuid = task.get("uuid");
        
        // Extract interval-related modifications from args
        for (const auto& arg : Context::getContext().cli2._args) {
          if (arg.hasTag("MODIFICATION") && arg._lextype == Lexer::Type::pair) {
            std::string name = arg.attribute("canonical");
            std::string value = arg.attribute("value");
            
            if (name == "start" || name == "stop" || name == "end") {
              interval_mods_processed = true;
              // Parse the datetime value using Variant (handles relative durations like -1h, +30min)
              try {
                // Get current interval to determine base time for relative durations
                std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);
                time_t base_time = 0;
                bool is_relative = false;
                
                // Find the current interval to get base time
                for (const auto& interval : intervals) {
                  if (interval.interval_id == interval_id) {
                    if (name == "start") {
                      base_time = interval.start_time;
                    } else {
                      // For stop/end, use end_time or current time if open
                      base_time = interval.is_open ? time(nullptr) : interval.end_time;
                    }
                    break;
                  }
                }
                
                if (base_time == 0) {
                  throw std::string(format("Interval with ID {1} not found for task {2}.", interval_id, task_uuid));
                }
                
                // Check if value looks like a relative duration (starts with + or -)
                std::string trimmed_value = value;
                while (trimmed_value.length() > 0 && (trimmed_value[0] == ' ' || trimmed_value[0] == '\t')) {
                  trimmed_value = trimmed_value.substr(1);
                }
                bool is_negative = (trimmed_value.length() > 0 && trimmed_value[0] == '-');
                bool is_positive = (trimmed_value.length() > 0 && trimmed_value[0] == '+');
                is_relative = (is_negative || is_positive);
                
                Variant evaluatedValue;
                time_t timestamp;
                
                if (is_relative) {
                  // For relative durations, parse the duration part (without the sign)
                  std::string duration_str = trimmed_value;
                  if (is_negative || is_positive) {
                    duration_str = trimmed_value.substr(1); // Remove leading + or -
                  }
                  
                  // Check if duration ends with just "m" - this is ambiguous (month vs minute)
                  if (duration_str.length() > 0 && duration_str[duration_str.length() - 1] == 'm') {
                    // Check if it's just a number followed by 'm' (e.g., "5m", "30m")
                    bool is_just_m = true;
                    for (size_t i = 0; i < duration_str.length() - 1; ++i) {
                      if (!std::isdigit(duration_str[i]) && duration_str[i] != '.' && duration_str[i] != '+' && duration_str[i] != '-') {
                        is_just_m = false;
                        break;
                      }
                    }
                    if (is_just_m) {
                      // "m" is ambiguous - could mean month or minute
                      throw std::string(format("Ambiguous duration unit 'm'. Use 'min' for minutes or 'mo' for months. Example: '{1}min' or '{1}mo'.", duration_str.substr(0, duration_str.length() - 1)));
                    }
                  }
                  
                  // Parse duration directly using Duration class
                  // Check if parse succeeded by testing parse result
                  std::string::size_type parse_pos = 0;
                  Duration test_duration;
                  bool parse_succeeded = test_duration.parse(duration_str, parse_pos) && parse_pos == duration_str.length();
                  
                  if (parse_succeeded) {
                    // Parse succeeded - use the parsed duration
                    time_t duration_seconds = test_duration.toTime_t();
                    // duration_seconds should be the duration in seconds
                    // Apply the sign - if negative, make duration negative
                    if (is_negative) {
                      duration_seconds = -duration_seconds;
                    }
                    // Add the duration to the base time
                    timestamp = base_time + duration_seconds;
                  } else {
                    // Parse failed, try Variant/Eval
                    try {
                      Variant durationValue(duration_str);
                      durationValue.cast(Variant::type_duration);
                      time_t duration_seconds = durationValue.get_duration();
                      if (is_negative) {
                        duration_seconds = -duration_seconds;
                      }
                      timestamp = base_time + duration_seconds;
                    } catch (const std::string& e) {
                      throw e;
                    } catch (...) {
                      throw std::string(format("Cannot parse duration '{1}'.", value));
                    }
                  }
                } else {
                  // Parse as absolute date/time
                  try {
                    Eval e;
                    e.addSource(domSource);
                    e.evaluateInfixExpression(value, evaluatedValue);
                  } catch (...) {
                    evaluatedValue = Variant(value);
                  }
                  
                  // Convert to date (handles duration -> date conversion)
                  if (evaluatedValue.type() == Variant::type_duration) {
                    evaluatedValue.cast(Variant::type_date);
                  } else {
                    evaluatedValue.cast(Variant::type_date);
                  }
                  
                  timestamp = evaluatedValue.get_date();
                  if (timestamp == 0 && value != "") {
                    throw std::string(format("'{1}' is not a valid date or duration.", value));
                  }
                }
                
                // Apply interval modification
                WorkInterval::modify_interval(task_uuid, interval_id, name, timestamp);
                feedback_affected(format("Modified interval {1}.{2} {3} time.", task_id, interval_id, name));
              } catch (const std::string& e) {
                throw e;
              } catch (const std::exception& e) {
                throw std::string(format("Cannot modify interval: {1}", e.what()));
              }
            }
          }
        }
      }
    }
    
    // If we processed interval modifications, return early (don't process task modifications)
    if (interval_mods_processed) {
      return rc;
    }
    
    // If we're in interval context but no interval mods were found, error out
    // Interval context only supports start:, stop:, and end: modifications
    Context::getContext().footnote("In interval context, only 'start:', 'stop:', or 'end:' modifications are allowed. Use 'task <id>.<interval-id> modify start:<time>' or similar.");
    return 1;
  }

  auto count = 0;
  if (filtered.size() > 1) {
    feedback_affected("This command will alter {1} tasks.", filtered.size());
  }
  for (auto& task : filtered) {
    Task before(task);
    
    // If in interval context, skip start/stop/end attributes for this task
    bool skip_interval_attrs = in_interval_context && 
                                Context::getContext().cli2._interval_ids.find(task.id) != 
                                Context::getContext().cli2._interval_ids.end();
    
    if (skip_interval_attrs) {
      // Temporarily remove interval attributes from processing
      // We'll handle this by modifying Task::modify to check interval context
      // For now, let's process normally but the interval mods are already done
    }
    
    task.modify(Task::modReplace);

    if (before != task) {
      // Abort if change introduces inconsistencies.
      checkConsistency(before, task);

      auto question =
          format("Modify task {1} '{2}'?", task.identifier(true), task.get("description"));

      if (permission(before.diff(task) + question, filtered.size())) {
        count += modifyAndUpdate(before, task, &projectChanges);
      } else {
        std::cout << "Task not modified.\n";
        rc = 1;
        if (_permission_quit) break;
      }
    }
  }

  // Now list the project changes.
  for (const auto& change : projectChanges)
    if (change.first != "") Context::getContext().footnote(change.second);

  feedback_affected(count == 1 ? "Modified {1} task." : "Modified {1} tasks.", count);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// TODO Why is this not in Task::validate?
void CmdModify::checkConsistency(Task& before, Task& after) {
  // Perform some logical consistency checks.
  if (after.has("recur") && !after.has("due") && !before.has("due"))
    throw std::string("You cannot specify a recurring task without a due date.");

  if (before.has("recur") && before.has("due") && (!after.has("due") || after.get("due") == ""))
    throw std::string("You cannot remove the due date from a recurring task.");

  if (before.has("recur") && (!after.has("recur") || after.get("recur") == ""))
    throw std::string("You cannot remove the recurrence from a recurring task.");

  if ((before.getStatus() == Task::pending) && (after.getStatus() == Task::pending) &&
      (before.get("end") == "") && (after.get("end") != ""))
    throw format("Could not modify task {1}. You cannot set an end date on a pending task.",
                 before.identifier(true));
}

////////////////////////////////////////////////////////////////////////////////
int CmdModify::modifyAndUpdate(Task& before, Task& after,
                               std::map<std::string, std::string>* projectChanges /* = NULL */) {
  // This task.
  auto count = 1;

  updateRecurrenceMask(after);
  feedback_affected("Modifying task {1} '{2}'.", after);
  feedback_unblocked(after);
  Context::getContext().tdb2.modify(after);
  if (Context::getContext().verbose("project") && projectChanges)
    (*projectChanges)[after.get("project")] = onProjectChange(before, after);

  // Task has siblings - modify them.
  if (after.has("parent")) count += modifyRecurrenceSiblings(after, projectChanges);

  // Task has child tasks - modify them.
  else if (after.get("status") == "recurring")
    count += modifyRecurrenceParent(after, projectChanges);

  return count;
}

////////////////////////////////////////////////////////////////////////////////
int CmdModify::modifyRecurrenceSiblings(
    Task& task, std::map<std::string, std::string>* projectChanges /* = NULL */) {
  auto count = 0;

  if ((Context::getContext().config.get("recurrence.confirmation") == "prompt" &&
       confirm(STRING_CMD_MODIFY_RECUR)) ||
      Context::getContext().config.getBoolean("recurrence.confirmation")) {
    std::vector<Task> siblings = Context::getContext().tdb2.siblings(task);
    for (auto& sibling : siblings) {
      Task alternate(sibling);
      sibling.modify(Task::modReplace);
      updateRecurrenceMask(sibling);
      ++count;
      feedback_affected(STRING_CMD_MODIFY_TASK_R, sibling);
      feedback_unblocked(sibling);
      Context::getContext().tdb2.modify(sibling);
      if (Context::getContext().verbose("project") && projectChanges)
        (*projectChanges)[sibling.get("project")] = onProjectChange(alternate, sibling);
    }

    // Modify the parent
    Task parent;
    Context::getContext().tdb2.get(task.get("parent"), parent);
    parent.modify(Task::modReplace);
    Context::getContext().tdb2.modify(parent);
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
int CmdModify::modifyRecurrenceParent(
    Task& task, std::map<std::string, std::string>* projectChanges /* = NULL */) {
  auto count = 0;

  auto children = Context::getContext().tdb2.children(task);
  if (children.size() &&
      ((Context::getContext().config.get("recurrence.confirmation") == "prompt" &&
        confirm(STRING_CMD_MODIFY_RECUR)) ||
       Context::getContext().config.getBoolean("recurrence.confirmation"))) {
    for (auto& child : children) {
      Task alternate(child);
      child.modify(Task::modReplace);
      updateRecurrenceMask(child);
      Context::getContext().tdb2.modify(child);
      if (Context::getContext().verbose("project") && projectChanges)
        (*projectChanges)[child.get("project")] = onProjectChange(alternate, child);
      ++count;
      feedback_affected(STRING_CMD_MODIFY_TASK_R, child);
    }
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
