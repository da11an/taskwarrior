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

#include <CmdSwitch.h>
#include <Context.h>
#include <Filter.h>
#include <WorkInterval.h>
#include <dependency.h>
#include <feedback.h>
#include <format.h>
#include <nag.h>
#include <recur.h>
#include <util.h>

#include <iostream>
#include <ctime>

////////////////////////////////////////////////////////////////////////////////
CmdSwitch::CmdSwitch() {
  _keyword = "switch";
  _usage = "task <filter> switch [<mods>]";
  _description = "Stops all active tasks and starts the specified task";
  _read_only = false;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = true;
  _accepts_filter = true;
  _accepts_modifications = true;
  _accepts_miscellaneous = false;
  _category = Command::Category::operation;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSwitch::execute(std::string&) {
  int rc = 0;
  int stopped_count = 0;
  int started_count = 0;

  // First, apply filter to find target task(s)
  Filter filter;
  std::vector<Task> filtered;
  filter.subset(filtered);
  if (filtered.size() == 0) {
    Context::getContext().footnote("No tasks specified.");
    return 1;
  }

  // Find all active tasks
  // Get all pending tasks and filter for those with "start" attribute
  std::vector<Task> allPending = Context::getContext().tdb2.pending_tasks();
  std::vector<Task> activeTasks;
  for (auto& task : allPending) {
    // Check if task is active (has start attribute and is not waiting)
    if (task.has("start") && !task.is_waiting()) {
      activeTasks.push_back(task);
    }
  }

  // Check if target task is already the only active task
  if (activeTasks.size() == 1 && filtered.size() == 1) {
    std::string active_uuid = activeTasks[0].get("uuid");
    std::string target_uuid = filtered[0].get("uuid");
    if (active_uuid == target_uuid) {
      std::cout << format("Task {1} '{2}' is already active.", filtered[0].identifier(true),
                          filtered[0].get("description"))
                << '\n';
      return 0;
    }
  }

  // Accumulated project change notifications.
  std::map<std::string, std::string> projectChanges;

  // Stop all active tasks
  if (activeTasks.size() > 0) {
    for (auto& task : activeTasks) {
      Task before(task);

      // Get stop time before removing start
      time_t stop_time = time(nullptr);

      task.modify(Task::modAnnotate);
      task.remove("start");

      if (Context::getContext().config.getBoolean("journal.time"))
        task.addAnnotation(Context::getContext().config.get("journal.time.stop.annotation"));

      // Stop without confirmation prompt (this is part of switch operation)
      updateRecurrenceMask(task);
      Context::getContext().tdb2.modify(task);

      // Log work interval stop event
      try {
        WorkInterval::log_event(task.get("uuid"), "stop", stop_time);
      } catch (const std::string& e) {
        Context::getContext().debug(format("Failed to log work interval: {1}", e));
      } catch (...) {
        Context::getContext().debug("Failed to log work interval: unknown error");
      }

      ++stopped_count;
      feedback_affected("Stopping task {1} '{2}'.", task);
      dependencyChainOnStart(task);
      if (Context::getContext().verbose("project"))
        projectChanges[task.get("project")] = onProjectChange(task, false);
    }
  }

  if (filtered.size() > 1) {
    feedback_affected("This command will alter {1} tasks.", filtered.size());
  }

  std::vector<Task> modified;
  for (auto& task : filtered) {
    // Check if this task was just stopped - if so, we'll restart it
    bool was_just_stopped = false;
    for (const auto& stopped : activeTasks) {
      if (stopped.get("uuid") == task.get("uuid")) {
        was_just_stopped = true;
        break;
      }
    }

    // If task already has start and wasn't just stopped, it means it's still active
    // (shouldn't happen if we stopped all active tasks, but handle it)
    if (task.has("start") && !was_just_stopped) {
      // This shouldn't normally happen, but handle gracefully
      Task before(task);
      time_t stop_time = time(nullptr);
      task.modify(Task::modAnnotate);
      task.remove("start");
      if (Context::getContext().config.getBoolean("journal.time"))
        task.addAnnotation(Context::getContext().config.get("journal.time.stop.annotation"));
      updateRecurrenceMask(task);
      Context::getContext().tdb2.modify(task);
      try {
        WorkInterval::log_event(task.get("uuid"), "stop", stop_time);
      } catch (const std::string& e) {
        Context::getContext().debug(format("Failed to log work interval: {1}", e));
      } catch (...) {
        Context::getContext().debug("Failed to log work interval: unknown error");
      }
    }

    Task before(task);

    // Start the specified task.
    std::string question =
        format("Start task {1} '{2}'?", task.identifier(true), task.get("description"));
    task.setAsNow("start");

    Task::status status = task.getStatus();
    if (status == Task::completed || status == Task::deleted) {
      task.setStatus(Task::pending);
    }

    task.modify(Task::modAnnotate);
    if (Context::getContext().config.getBoolean("journal.time"))
      task.addAnnotation(Context::getContext().config.get("journal.time.start.annotation"));

    if (permission(before.diff(task) + question, filtered.size())) {
      updateRecurrenceMask(task);
      Context::getContext().tdb2.modify(task);

      // Log work interval start event
      try {
        time_t start_time = task.get_date("start");
        WorkInterval::log_event(task.get("uuid"), "start", start_time);
      } catch (const std::string& e) {
        Context::getContext().debug(format("Failed to log work interval: {1}", e));
      }

      ++started_count;
      feedback_affected("Starting task {1} '{2}'.", task);
      dependencyChainOnStart(task);
      if (Context::getContext().verbose("project"))
        projectChanges[task.get("project")] = onProjectChange(task, false);

      // Save unmodified task for potential nagging later
      modified.push_back(before);
    } else {
      std::cout << "Task not started.\n";
      rc = 1;
      if (_permission_quit) break;
    }
  }

  nag(modified);

  // Now list the project changes.
  for (auto& change : projectChanges)
    if (change.first != "") Context::getContext().footnote(change.second);

  // Provide feedback on what was done
  if (stopped_count > 0) {
    feedback_affected(stopped_count == 1 ? "Stopped {1} task." : "Stopped {1} tasks.", stopped_count);
  }
  if (started_count > 0) {
    feedback_affected(started_count == 1 ? "Started {1} task." : "Started {1} tasks.", started_count);
  } else if (stopped_count == 0) {
    // No tasks were stopped and none were started - this shouldn't normally happen
    Context::getContext().footnote("No tasks were switched.");
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
