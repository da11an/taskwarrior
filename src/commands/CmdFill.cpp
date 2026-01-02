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

#include <CmdFill.h>
#include <Context.h>
#include <Filter.h>
#include <WorkInterval.h>
#include <feedback.h>
#include <format.h>
#include <shared.h>

////////////////////////////////////////////////////////////////////////////////
CmdFill::CmdFill() {
  _keyword = "fill";
  _usage = "task <task-id>.<interval-id> fill [start|stop|both]";
  _description = "Fills an interval to meet neighboring intervals (task-agnostic)";
  _read_only = false;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = false;
  _accepts_filter = true;
  _accepts_modifications = false;
  _accepts_miscellaneous = true;  // Accept fill type argument
  _category = Command::Category::operation;
}

////////////////////////////////////////////////////////////////////////////////
int CmdFill::execute(std::string&) {
  int rc = 0;

  // Apply filter to get tasks
  Filter filter;
  std::vector<Task> filtered;
  filter.subset(filtered);

  if (filtered.size() == 0) {
    Context::getContext().footnote("No tasks specified.");
    return 1;
  }

  // Check if we're in interval context
  if (Context::getContext().cli2._interval_ids.empty()) {
    Context::getContext().footnote("Fill command requires interval syntax: task <id>.<interval-id> fill");
    return 1;
  }

  // Get fill type from miscellaneous arguments
  std::string fill_type = "both";  // Default
  auto words = Context::getContext().cli2.getWords();
  if (!words.empty()) {
    std::string first_word = words[0];
    if (first_word == "start" || first_word == "stop" || first_word == "both") {
      fill_type = first_word;
    } else {
      Context::getContext().footnote(format("Invalid fill type '{1}'. Use 'start', 'stop', or 'both'.", first_word));
      return 1;
    }
  }

  int count = 0;
  for (auto& task : filtered) {
    int task_id = task.id;
    auto it = Context::getContext().cli2._interval_ids.find(task_id);
    if (it != Context::getContext().cli2._interval_ids.end()) {
      int interval_id = it->second;
      std::string task_uuid = task.get("uuid");

      try {
        WorkInterval::fill_interval(task_uuid, interval_id, fill_type);
        feedback_affected(format("Filled interval {1}.{2} ({3}).", task_id, interval_id, fill_type));
        ++count;
      } catch (const std::string& e) {
        Context::getContext().footnote(e);
        rc = 1;
      } catch (const std::exception& e) {
        Context::getContext().footnote(format("Cannot fill interval: {1}", e.what()));
        rc = 1;
      }
    } else {
      Context::getContext().footnote(format("Task {1} does not have an interval ID specified.", task_id));
      rc = 1;
    }
  }

  if (count > 0) {
    feedback_affected(format(count == 1 ? "Filled {1} interval." : "Filled {1} intervals.", count));
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
