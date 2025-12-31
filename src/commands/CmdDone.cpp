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

#include <CmdDone.h>
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
CmdDone::CmdDone() {
  _keyword = "done";
  _usage = "task <filter> done <mods>";
  _description = "Marks the specified task as completed";
  _read_only = false;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = true;
  _accepts_filter = true;
  _accepts_modifications = true;
  _accepts_miscellaneous = true;  // Allow --message/-m flag
  _category = Command::Category::operation;
}

////////////////////////////////////////////////////////////////////////////////
int CmdDone::execute(std::string&) {
  auto rc = 0;
  auto count = 0;

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

  // Extract message from CLI2
  std::string message = Context::getContext().cli2.getMessage();

  if (filtered.size() > 1) {
    feedback_affected("This command will alter {1} tasks.", filtered.size());
  }

  std::vector<Task> modified;
  for (auto& task : filtered) {
    Task before(task);

    if (task.getStatus() == Task::pending || task.getStatus() == Task::waiting) {
      // Complete the specified task.
      std::string question =
          format("Complete task {1} '{2}'?", task.identifier(true), task.get("description"));

      // Get start time before removing it (if exists)
      time_t start_time = 0;
      time_t done_time = time(nullptr);
      if (task.has("start")) {
        start_time = task.get_date("start");
      }

      task.modify(Task::modAnnotate);
      task.setStatus(Task::completed);
      if (!task.has("end")) task.setAsNow("end");

      // Stop the task, if started.
      if (task.has("start")) {
        task.remove("start");
        if (Context::getContext().config.getBoolean("journal.time"))
          task.addAnnotation(Context::getContext().config.get("journal.time.stop.annotation"));
      }

      // Add work interval message as annotation with interval timestamp
      if (!message.empty() && start_time > 0) {
        task.addAnnotation(message, done_time);
      }

      if (permission(before.diff(task) + question, filtered.size())) {
        updateRecurrenceMask(task);
        Context::getContext().tdb2.modify(task);
        
        // Log work interval done event (without message - it's in annotation now)
        if (start_time > 0) {
          try {
            WorkInterval::log_event(task.get("uuid"), "done", done_time);
          } catch (const std::string& e) {
            // Log error but don't fail the command
            Context::getContext().debug(format("Failed to log work interval: {1}", e));
          }
        }
        
        ++count;
        feedback_affected("Completed task {1} '{2}'.", task);
        feedback_unblocked(task);
        dependencyChainOnComplete(task);
        if (Context::getContext().verbose("project"))
          projectChanges[task.get("project")] = onProjectChange(task);

        // Save unmodified task for potential nagging later
        modified.push_back(before);
      } else {
        std::cout << "Task not completed.\n";
        rc = 1;
        if (_permission_quit) break;
      }
    } else {
      std::cout << format("Task {1} '{2}' is neither pending nor waiting.", task.identifier(true),
                          task.get("description"))
                << '\n';
      rc = 1;
    }
  }

  nag(modified);

  // Now list the project changes.
  for (const auto& change : projectChanges)
    if (change.first != "") Context::getContext().footnote(change.second);

  feedback_affected(count == 1 ? "Completed {1} task." : "Completed {1} tasks.", count);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
