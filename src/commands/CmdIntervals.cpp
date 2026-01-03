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

#include <CmdIntervals.h>
#include <Context.h>
#include <Datetime.h>
#include <Duration.h>
#include <Filter.h>
#include <Table.h>
#include <WorkInterval.h>
#include <format.h>
#include <shared.h>
#include <utf8.h>

#include <iomanip>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
CmdIntervals::CmdIntervals() {
  _keyword = "intervals";
  _usage = "task <filter> intervals";
  _description = "Shows work intervals for matching tasks";
  _read_only = true;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = true;
  _accepts_filter = true;
  _accepts_modifications = false;
  _accepts_miscellaneous = false;
  _category = Command::Category::metadata;
}

////////////////////////////////////////////////////////////////////////////////
int CmdIntervals::execute(std::string& output) {
  int rc = 0;

  // Apply filter.
  Filter filter;
  std::vector<Task> filtered;
  filter.subset(filtered);

  if (filtered.size() == 0) {
    Context::getContext().footnote("No tasks specified.");
    return 1;
  }

  // Get date format (prefer dateformat.info, fallback to dateformat, then default with time)
  auto dateformat = Context::getContext().config.get("dateformat.info");
  if (dateformat == "") dateformat = Context::getContext().config.get("dateformat");
  if (dateformat == "") dateformat = "Y-M-D H:N:S";

  std::stringstream out;
  Table view;
  view.width(Context::getContext().getWidth());
  view.withColor(Context::getContext().color());

  // Header
  view.add("ID", false);      // Right-align ID (format: task.interval)
  view.add("Description");
  view.add("Start");
  view.add("End");
  view.add("Duration", false); // Right-align Duration
  view.add("Start Note");
  view.add("During");
  view.add("End Note");
  view.intraPadding(2);

  bool has_intervals = false;
  time_t total_duration = 0;

  for (auto& task : filtered) {
    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    if (intervals.empty()) {
      continue;
    }

    has_intervals = true;

    // Get annotations for this task to match with interval timestamps
    auto annotations = task.getAnnotations();
    
    // Get current time once for all intervals (used for open interval annotation filtering)
    time_t current_time = time(nullptr);

    for (const auto& interval : intervals) {
      int row = view.addRow();
      
      // Set ID (column 0) - format: task.interval
      // For completed tasks (id == 0), use "-" as placeholder
      std::string task_id_str;
      if (task.id != 0) {
        task_id_str = format("{1}", task.id);
      } else {
        // Use "-" for completed/deleted tasks (no longer active tasks)
        task_id_str = "-";
      }
      view.set(row, 0, format("{1}.{2}", task_id_str, interval.interval_id));
      
      // Set Description (column 1) - 20 character snippet
      std::string description = task.get("description");
      std::string short_desc;
      int desc_width = utf8_width(description);
      if (desc_width > 20) {
        // Truncate to fit exactly 20 characters: use 19 chars + 1 char for ellipsis
        // The ellipsis uses the 20th character's space
        short_desc = utf8_substr(description, 0, 19) + "…";  // Single ellipsis character
        // Verify the total width is exactly 20
        int short_width = utf8_width(short_desc);
        if (short_width != 20) {
          // Adjust if needed: truncate to (20 - ellipsis_width) characters
          int ellipsis_width = utf8_width("…");
          int target_length = 20 - ellipsis_width;
          short_desc = utf8_substr(description, 0, target_length) + "…";
        }
      } else {
        short_desc = description;
      }
      view.set(row, 1, short_desc);
      
      // Format start time (column 2)
      Datetime start_dt(interval.start_time);
      view.set(row, 2, start_dt.toString(dateformat));
      
      // Format end time (column 3)
      // For open intervals, show "ongoing" or current time
      if (interval.is_open) {
        view.set(row, 3, "ongoing");
      } else {
        Datetime end_dt(interval.end_time);
        view.set(row, 3, end_dt.toString(dateformat));
      }
      
      // Calculate and format duration (column 4)
      time_t duration_sec = interval.duration();
      total_duration += duration_sec;
      Duration duration(duration_sec);
      view.set(row, 4, duration.format());
      
      // Collect all annotations within this interval
      // Annotations are stored as annotation_<timestamp>
      std::vector<std::string> start_notes;
      std::vector<std::string> during_notes;
      std::vector<std::string> end_notes;
      
      // Tolerance for "at start" and "at end" (within 5 seconds to account for annotation timestamp increments)
      const time_t tolerance = 5;
      time_t interval_duration = interval.end_time - interval.start_time;
      
      for (const auto& anno : annotations) {
        // Extract timestamp from annotation key (annotation_<timestamp>)
        if (anno.first.substr(0, 11) == "annotation_") {
          time_t anno_time = strtoll(anno.first.substr(11).c_str(), nullptr, 10);
          
          // For open intervals, only include annotations up to current time
          time_t effective_end_time = interval.is_open ? current_time : interval.end_time;
          
          // Check if annotation is within the interval (with some tolerance for edge cases)
          // Include annotations slightly before start (journal annotations) and slightly after end
          if (anno_time >= interval.start_time - tolerance && anno_time <= effective_end_time + tolerance) {
            // For very short intervals (< 10 seconds), use simpler logic
            if (interval_duration < 10) {
              // If very close to end (within tolerance), it's an end note
              if (anno_time >= interval.end_time - tolerance) {
                end_notes.push_back(anno.second);
              }
              // Otherwise, if close to start, it's a start note
              else if (anno_time <= interval.start_time + tolerance) {
                start_notes.push_back(anno.second);
              }
              // Everything else is during
              else {
                during_notes.push_back(anno.second);
              }
            } else {
              // For longer intervals, use more precise categorization
              time_t distance_from_start = anno_time - interval.start_time;
              time_t effective_end_time = interval.is_open ? current_time : interval.end_time;
              time_t distance_from_end = effective_end_time - anno_time;
              
              // Prioritize end notes if annotation is closer to end than start
              if (distance_from_end <= tolerance && distance_from_end <= distance_from_start) {
                end_notes.push_back(anno.second);
              }
              // Start notes if closer to start
              else if (distance_from_start <= tolerance && distance_from_start < distance_from_end) {
                start_notes.push_back(anno.second);
              }
              // During: everything else in between
              else if (anno_time > interval.start_time + tolerance && anno_time < effective_end_time - tolerance) {
                during_notes.push_back(anno.second);
              }
            }
          }
        }
      }
      
      // Join multiple notes with semicolons
      auto join_notes = [](const std::vector<std::string>& notes) -> std::string {
        if (notes.empty()) return "";
        std::string result = notes[0];
        for (size_t i = 1; i < notes.size(); ++i) {
          result += "; " + notes[i];
        }
        return result;
      };
      
      // Set annotation columns (5, 6, 7)
      view.set(row, 5, join_notes(start_notes));
      view.set(row, 6, join_notes(during_notes));
      view.set(row, 7, join_notes(end_notes));
    }
  }

  if (!has_intervals) {
    Context::getContext().footnote("No work intervals found for matching tasks.");
    return 1;
  }

  out << view.render();
  
  // Show total duration if multiple intervals
  if (has_intervals) {
    Duration total(total_duration);
    out << '\n' << format("Total time: {1}", total.format()) << '\n';
  }

  output = out.str();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
