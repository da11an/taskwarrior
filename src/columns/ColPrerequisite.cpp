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

#include <ColPrerequisite.h>
#include <Context.h>
#include <format.h>
#include <shared.h>
#include <util.h>

#include <regex>

#define STRING_COLUMN_LABEL_PREREQ "Prerequisite"

////////////////////////////////////////////////////////////////////////////////
ColumnPrerequisite::ColumnPrerequisite() {
  _name = "prerequisite";
  _style = "list";
  _label = STRING_COLUMN_LABEL_PREREQ;
  _styles = {"list"};
  _examples = {"5", "5,6,7", "5-10"};
}

////////////////////////////////////////////////////////////////////////////////
// Overriden so that style <----> label are linked.
void ColumnPrerequisite::setStyle(const std::string& value) {
  Column::setStyle(value);
}

////////////////////////////////////////////////////////////////////////////////
// Set the minimum and maximum widths for the value.
// Prerequisite is a virtual attribute, so this is not typically used.
void ColumnPrerequisite::measure(Task&, unsigned int& minimum, unsigned int& maximum) {
  minimum = maximum = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Render the prerequisite value.
// Prerequisite is a virtual attribute, so this is not typically used.
void ColumnPrerequisite::render(std::vector<std::string>&, Task&, int, Color&) {
  // Prerequisite is a virtual attribute - it doesn't store data, only sets dependencies
  // So there's nothing to render
}

////////////////////////////////////////////////////////////////////////////////
void ColumnPrerequisite::modify(Task& task, const std::string& value) {
  // Ensure current task has a UUID (for new tasks, validate assigns UUID)
  // Note: validate() is typically called after modify(), but we need UUID now
  // So we ensure it exists here
  if (!task.has("uuid") || task.get("uuid") == "") {
    // For new tasks, assign UUID now (validate() will check it later)
    task.set("uuid", uuid());
  }
  std::string current_uuid = task.get("uuid");

  // Parse prerequisite list (similar to ColumnDepends::modify)
  for (auto& prereq : split(value, ',')) {
    bool removal = false;
    if (prereq[0] == '-') {
      removal = true;
      prereq = prereq.substr(1);
    }

    auto hyphen = prereq.find('-');
    long lower, upper;                                       // For ID ranges
    std::regex valid_uuid("[a-f0-9]{8}([a-f0-9-]{4,28})?");  // TODO: Make more precise

    // UUID
    if (prereq.length() >= 8 && std::regex_match(prereq, valid_uuid)) {
      // Full UUID, can be added directly
      if (prereq.length() == 36) {
        Task target_task;
        if (Context::getContext().tdb2.get(prereq, target_task)) {
          if (removal)
            target_task.removeDependency(current_uuid);
          else
            target_task.addDependency(current_uuid);

          // Save the modified target task
          Context::getContext().tdb2.modify(target_task);
        } else {
          throw format("Prerequisite could not be set - task with UUID '{1}' does not exist.", prereq);
        }
      }
      // Short UUID, need to look up full form
      else {
        Task target_task;
        if (Context::getContext().tdb2.get(prereq, target_task)) {
          if (removal)
            target_task.removeDependency(current_uuid);
          else
            target_task.addDependency(current_uuid);

          // Save the modified target task
          Context::getContext().tdb2.modify(target_task);
        } else {
          throw format("Prerequisite could not be set - task with UUID '{1}' does not exist.", prereq);
        }
      }
    }
    // ID range
    else if (prereq.find('-') != std::string::npos &&
             extractLongInteger(prereq.substr(0, hyphen), lower) &&
             extractLongInteger(prereq.substr(hyphen + 1), upper)) {
      for (long i = lower; i <= upper; i++) {
        std::string target_uuid = Context::getContext().tdb2.uuid(i);
        if (target_uuid == "") {
          throw format("Prerequisite could not be set - task {1} does not exist.", i);
        }

        Task target_task;
        if (Context::getContext().tdb2.get(target_uuid, target_task)) {
          if (removal)
            target_task.removeDependency(current_uuid);
          else
            target_task.addDependency(current_uuid);

          // Save the modified target task
          Context::getContext().tdb2.modify(target_task);
        } else {
          throw format("Prerequisite could not be set - task {1} does not exist.", i);
        }
      }
    }
    // Simple ID
    else if (extractLongInteger(prereq, lower)) {
      std::string target_uuid = Context::getContext().tdb2.uuid(lower);
      if (target_uuid == "") {
        throw format("Prerequisite could not be set - task {1} does not exist.", lower);
      }

      Task target_task;
      if (Context::getContext().tdb2.get(target_uuid, target_task)) {
        if (removal)
          target_task.removeDependency(current_uuid);
        else
          target_task.addDependency(current_uuid);

        // Save the modified target task
        Context::getContext().tdb2.modify(target_task);
      } else {
        throw format("Prerequisite could not be set - task {1} does not exist.", lower);
      }
    } else {
      throw format("Invalid prerequisite value: '{1}'", prereq);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
