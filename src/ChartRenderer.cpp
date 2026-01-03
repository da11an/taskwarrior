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
#include <ChartRenderer.h>
#include <Context.h>
#include <Color.h>
#include <Duration.h>
#include <Datetime.h>
#include <Task.h>
#include <WorkInterval.h>
#include <IntervalAggregator.h>
#include <format.h>
#include <utf8.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <tuple>

////////////////////////////////////////////////////////////////////////////////
std::string ChartRenderer::formatDuration(time_t seconds) {
  Duration d(seconds);
  return d.format();
}

////////////////////////////////////////////////////////////////////////////////
char ChartRenderer::getBlockChar(double intensity) {
  // intensity is 0.0 to 1.0
  if (intensity <= 0.0) return ' ';
  if (intensity < 0.25) return '░';
  if (intensity < 0.50) return '▒';
  if (intensity < 0.75) return '▓';
  return '█';
}

////////////////////////////////////////////////////////////////////////////////
Color ChartRenderer::getColorForItem(const std::string& identifier) {
  // Simple hash-based color assignment for consistency
  // This ensures the same project/task always gets the same color
  unsigned int hash = 0;
  for (char c : identifier) {
    hash = hash * 31 + c;
  }
  
  // Use a palette of colors
  const char* colors[] = {
    "blue", "green", "yellow", "magenta", "cyan", "red",
    "bright blue", "bright green", "bright yellow", "bright magenta",
    "bright cyan", "bright red"
  };
  
  int color_index = hash % (sizeof(colors) / sizeof(colors[0]));
  return Color(colors[color_index]);
}

////////////////////////////////////////////////////////////////////////////////
std::string ChartRenderer::renderBarChart(
  const std::vector<BarData>& data,
  int width,
  bool show_percentages
) {
  if (data.empty()) {
    return "No data to display.\n";
  }

  // Find maximum value for scaling
  time_t max_value = 0;
  time_t total = 0;
  for (const auto& bar : data) {
    if (bar.value > max_value) max_value = bar.value;
    total += bar.value;
  }

  if (max_value == 0) {
    return "No time data to display.\n";
  }

  // Calculate bar width (leave space for labels)
  int max_label_width = 0;
  for (const auto& bar : data) {
    int label_width = utf8_width(bar.label);
    if (label_width > max_label_width) max_label_width = label_width;
  }

  int bar_width = width - max_label_width - 20;  // Leave space for value/percentage
  if (bar_width < 10) bar_width = 10;

  std::stringstream out;
  
  for (const auto& bar : data) {
    // Calculate bar length
    int bar_length = (int)((double)bar.value / max_value * bar_width);
    
    // Build bar string (use '#' for ASCII compatibility)
    std::string bar_str(bar_length, '#');
    
    // Format label (pad to max_label_width)
    std::string label = bar.label;
    int label_width = utf8_width(label);
    if (label_width < max_label_width) {
      label += std::string(max_label_width - label_width, ' ');
    }
    
    // Format value
    std::string value_str = formatDuration(bar.value);
    
    // Build line
    std::string line = label + "  " + bar_str;
    
    // Add percentage if requested
    if (show_percentages && total > 0) {
      double percentage = (double)bar.value / total * 100.0;
      line += "  " + value_str + format(" ({1}%)", (int)percentage);
    } else {
      line += "  " + value_str;
    }
    
    // Apply color if enabled
    if (Context::getContext().color() && !bar.color_key.empty()) {
      Color c = getColorForItem(bar.color_key);
      line = c.colorize(line);
    }
    
    out << line << '\n';
  }

  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string ChartRenderer::renderCalendarHeatmap(
  const std::map<time_t, time_t>& daily_data,
  int weeks
) {
  if (daily_data.empty()) {
    return "No data to display.\n";
  }

  // Get date range
  time_t now = time(nullptr);
  time_t start_time = now - (weeks * 7 * 24 * 60 * 60);

  std::stringstream out;
  out << "Calendar Heatmap (Last " << weeks << " Weeks)\n";
  out << std::string(63, '=') << "\n";
  // Align weekday labels with day blocks (each day is 5 chars: "  ░░░")
  // Align weekday header with day blocks
  // Week label is 7 chars + 2 spaces = 9 chars, then each day block is "  ░░░" (5 chars: 2 spaces + 3 blocks)
  // The blocks (░) start at position 9 + 2 = 11, so "Mon" should start at position 11 to align with blocks
  // Each weekday is 3 chars, with 2 spaces between them (matching the day block spacing)
  // Week label is 9 chars + 2 spaces = 11 chars before day blocks
  // Day blocks are now 4 chars + 2 spaces gap = 6 chars total per day
  // So the block characters start at position 11 + 2 = 13
  // Shift header by 2 to align with the block characters: 11 + 2 = 13 spaces
  out << "             Mon   Tue   Wed   Thu   Fri   Sat   Sun\n";

  // Build a map: (year, month, day) -> duration using the EXACT same method as IntervalAggregator
  std::map<std::tuple<int, int, int>, time_t> date_to_duration;
  for (const auto& pair : daily_data) {
    Datetime dt(pair.first);
    // Use same method as IntervalAggregator: dt.year(), dt.month(), dt.day()
    date_to_duration[std::make_tuple(dt.year(), dt.month(), dt.day())] = pair.second;
  }

  // Find the Monday of the week containing start_time
  Datetime start_dt(start_time);
  int start_dow = start_dt.dayOfWeek();  // 0=Sunday, 1=Monday, ..., 6=Saturday
  // Convert to Monday=0, Tuesday=1, ..., Sunday=6
  int monday_based_dow = (start_dow == 0) ? 6 : start_dow - 1;
  time_t week_start = start_time - (monday_based_dow * 24 * 60 * 60);
  
  // Group by week
  time_t current_week_start = week_start;
  int week_num = 0;
  
  // Continue until we've shown the week containing 'now' or reached the week limit
  // Calculate the Monday of the week containing 'now' to know when to stop
  Datetime now_dt(now);
  int now_dow = now_dt.dayOfWeek();
  int now_monday_based_dow = (now_dow == 0) ? 6 : now_dow - 1;
  time_t now_week_start = now - (now_monday_based_dow * 24 * 60 * 60);
  
  // Show weeks until we've included the week containing 'now'
  // Allow showing one extra week if needed to include the current week
  while (current_week_start <= now_week_start + (7 * 24 * 60 * 60) && week_num <= weeks) {
    // Format week label as "Mon DD-DD" or "Mon DD-DD" (first month only if week spans months)
    // Maximum format: "Dec 29-31" = 9 characters (3 letter month + space + 2 digits + dash + 2 digits)
    Datetime week_start_dt(current_week_start);
    Datetime week_end_dt(current_week_start + (6 * 24 * 60 * 60));  // Sunday of the week
    
    std::string week_label;
    if (week_start_dt.month() == week_end_dt.month()) {
      // Same month: "Mon DD-DD"
      week_label = format("{1} {2}-{3}", 
                          Datetime::monthNameShort(week_start_dt.month()),
                          week_start_dt.day(),
                          week_end_dt.day());
    } else {
      // Different months: "Mon DD-DD" (only show first month)
      week_label = format("{1} {2}-{3}", 
                          Datetime::monthNameShort(week_start_dt.month()),
                          week_start_dt.day(),
                          week_end_dt.day());
    }
    
    // Pad to 9 characters to align with weekday header (max: "Dec 29-31")
    if (week_label.length() < 9) {
      week_label += std::string(9 - week_label.length(), ' ');
    }
    out << week_label << "  ";
    
    // For each day of week (Monday through Sunday)
    for (int day_offset = 0; day_offset < 7; ++day_offset) {
      time_t day_time = current_week_start + (day_offset * 24 * 60 * 60);
      
      // Get start of day (midnight) - use EXACTLY the same method as IntervalAggregator
      Datetime day_dt(day_time);
      Datetime day_start(day_dt.year(), day_dt.month(), day_dt.day(), 0, 0, 0);
      time_t day_epoch = day_start.toEpoch();
      
      // Check if this day is within the range
      // Use day_epoch for comparison, but allow days that overlap with the range
      // A day is in range if its start (day_epoch) is before 'now' and its end is after start_time
      time_t day_end_epoch = day_epoch + (24 * 60 * 60) - 1;  // End of day (23:59:59)
      if (day_epoch < now && day_end_epoch >= start_time) {
        time_t day_value = 0;
        // Try exact match first
        auto it = daily_data.find(day_epoch);
        if (it != daily_data.end()) {
          day_value = it->second;
        } else {
          // Fallback: lookup by calendar date using reverse map
          // Use same method as when building the map: dt.year(), dt.month(), dt.day()
          // CRITICAL: Use day_dt (not day_start) to match how we built the map
          auto date_it = date_to_duration.find(std::make_tuple(day_dt.year(), day_dt.month(), day_dt.day()));
          if (date_it != date_to_duration.end()) {
            day_value = date_it->second;
          }
        }
        
        // Determine intensity level based on duration
        // _ = underscore, G = gray/dotted (░), W = white/solid (█)
        // 4 characters per day: ____ = <30min, G___ = 1h, GG__ = 2h, GGG_ = 3h, GGGG = 4h, WGGG = 5h, WWGG = 6h, WWWG = 7h, WWWW = 8h+
        std::string block_str;
        if (day_value < 30 * 60) {  // Less than 30 minutes
          block_str = "____";  // ____ (<30min)
        } else if (day_value < 1.5 * 3600) {  // Less than 1.5 hours (rounds to 1h)
          block_str = "░___";  // G___ (1h)
        } else if (day_value < 2.5 * 3600) {  // Less than 2.5 hours (rounds to 2h)
          block_str = "░░__";  // GG__ (2h)
        } else if (day_value < 3.5 * 3600) {  // Less than 3.5 hours (rounds to 3h)
          block_str = "░░░_";  // GGG_ (3h)
        } else if (day_value < 4.5 * 3600) {  // Less than 4.5 hours (rounds to 4h)
          block_str = "░░░░";  // GGGG (4h)
        } else if (day_value < 5.5 * 3600) {  // Less than 5.5 hours (rounds to 5h)
          block_str = "█░░░";  // WGGG (5h)
        } else if (day_value < 6.5 * 3600) {  // Less than 6.5 hours (rounds to 6h)
          block_str = "██░░";  // WWGG (6h)
        } else if (day_value < 7.5 * 3600) {  // Less than 7.5 hours (rounds to 7h)
          block_str = "███░";  // WWWG (7h)
        } else {  // 7.5+ hours (8h+)
          block_str = "████";  // WWWW (8h+)
        }
        
        out << "  " << block_str;
      } else {
        // Out of range: use blank to maintain column readability (4 chars + 2 gap spaces = 6 spaces)
        out << "      ";
      }
    }
    
    out << "\n";
    week_num++;
    current_week_start += 7 * 24 * 60 * 60;  // Move to next Monday
  }

  out << "\nLegend:  ____ = 0h  ░___ = 1h  ░░__ = 2h  ░░░_ = 3h  ░░░░ = 4h\n";
  out << "                    █░░░ = 5h  ██░░ = 6h  ███░ = 7h  ████ = 8h+\n";
  
  // Calculate and display total time
  time_t total_time = 0;
  for (const auto& pair : daily_data) {
    total_time += pair.second;
  }
  if (total_time > 0) {
    Duration total_dur(total_time);
    out << "Total: " << total_dur.format() << "\n";
  }

  out << std::string(63, '-') << "\n";
  
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string ChartRenderer::renderTimeline(
  const std::vector<class Task>& tasks,
  time_t start_time,
  time_t end_time,
  const std::string& granularity
) {
  std::stringstream out;
  
  // Get all intervals for the tasks in the date range
  DateRange range;
  range.start = start_time;
  range.end = end_time;
  
  std::vector<Interval> all_intervals = IntervalAggregator::getAllIntervals(tasks, range);
  
  if (all_intervals.empty()) {
    out << "Timeline View\n";
    out << std::string(60, '=') << "\n";
    out << "No work intervals found in the specified time range.\n";
    return out.str();
  }
  
  // Group tasks by project, then by task
  std::map<std::string, std::vector<Task>> tasks_by_project;
  std::map<std::string, std::vector<Interval>> intervals_by_task;
  std::map<std::string, Task> task_map;
  
  for (const auto& task : tasks) {
    std::string task_uuid = task.get("uuid");
    std::string project = task.get("project");
    if (project == "") project = "Uncategorized";
    
    task_map[task_uuid] = task;
    tasks_by_project[project].push_back(task);
    
    std::vector<Interval> task_intervals = WorkInterval::get_intervals(task_uuid);
    for (const auto& interval : task_intervals) {
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;
      
      if (interval_end >= start_time && interval_start <= end_time) {
        intervals_by_task[task_uuid].push_back(interval);
      }
    }
  }
  
  if (intervals_by_task.empty()) {
    out << "Timeline View\n";
    out << std::string(60, '=') << "\n";
    out << "No work intervals found in the specified time range.\n";
    return out.str();
  }
  
  // Find maximum task ID to determine padding width
  int max_task_id = 0;
  for (const auto& task : tasks) {
    if (task.id > max_task_id) {
      max_task_id = task.id;
    }
  }
  
  // Calculate number of digits needed for task ID padding
  int id_digits = 1;
  if (max_task_id > 0) {
    id_digits = 1 + (int)log10((double)max_task_id);
  }
  
  // Calculate timeline parameters
  // For timeline view, we'll show last 7 days (or actual range if shorter)
  // Show chronologically from oldest to newest (left to right)
  Datetime end_dt(end_time);
  Datetime visible_start_dt(end_dt.year(), end_dt.month(), end_dt.day(), 0, 0, 0);
  visible_start_dt = visible_start_dt - (6 * 24 * 60 * 60);  // 7 days total (today + 6 days back)
  time_t visible_start = std::max(visible_start_dt.toEpoch(), start_time);
  time_t visible_end = end_time;
  
  // Static 4-hour blocks: 0-4, 4-8, 8-12, 12-16, 16-20, 20-24
  // Block indices: 0=0-4, 1=4-8, 2=8-12, 3=12-16, 4=16-20, 5=20-24
  const int BLOCK_SIZE = 4;  // 4 hours per block
  const int NUM_BLOCKS = 6;  // 6 blocks per day
  
  // For each day, determine which blocks have intervals
  // We'll calculate this per day and show only blocks with intervals and blocks between them
  // Default to showing blocks 2 and 3 (8-12 and 12-16) if no intervals
  
  // label_width = id_digits (padded) + 2 (": ") + 20 (description) + 1 (indent) = id_digits + 23
  int label_width = id_digits + 23;
  int max_days = 7;  // Limit to 7 days for readability
  
  // Calculate actual number of days to show
  int actual_days = 0;
  Datetime day_start_check(end_dt.year(), end_dt.month(), end_dt.day(), 0, 0, 0);
  std::vector<time_t> day_starts;
  while (day_start_check.toEpoch() >= visible_start && actual_days < max_days) {
    day_starts.push_back(day_start_check.toEpoch());
    actual_days++;
    day_start_check = day_start_check - (24 * 60 * 60);
  }
  // Reverse to get chronological order (oldest to newest)
  std::reverse(day_starts.begin(), day_starts.end());
  
  // For each day, determine which blocks have intervals
  // Structure: day_index -> set of block indices that have intervals
  std::vector<std::set<int>> blocks_with_intervals_per_day(actual_days);
  
  for (const auto& interval : all_intervals) {
    time_t interval_start = std::max(interval.start_time, visible_start);
    time_t interval_end = std::min(interval.is_open ? time(nullptr) : interval.end_time, visible_end);
    if (interval_end <= interval_start) continue;
    
    // Find which days and blocks this interval spans
    Datetime start_dt(interval_start);
    Datetime end_dt(interval_end);
    
    // Get the day index for start and end
    for (size_t day_idx = 0; day_idx < day_starts.size(); ++day_idx) {
      time_t day_start = day_starts[day_idx];
      time_t day_end = day_start + (24 * 3600);
      
      // Check if interval overlaps with this day
      if (interval_end > day_start && interval_start < day_end) {
        // Calculate which blocks this interval touches in this day
        time_t day_interval_start = std::max(interval_start, day_start);
        time_t day_interval_end = std::min(interval_end, day_end);
        
        Datetime day_start_dt(day_interval_start);
        Datetime day_end_dt(day_interval_end);
        
        int start_hour = day_start_dt.hour();
        int end_hour = day_end_dt.hour();
        if (day_interval_end > day_start + (end_hour * 3600)) {
          // If we're past the hour boundary, include the next hour
          end_hour++;
        }
        
        // Determine which blocks are touched
        for (int hour = start_hour; hour < end_hour; ++hour) {
          int block = hour / BLOCK_SIZE;
          if (block >= 0 && block < NUM_BLOCKS) {
            blocks_with_intervals_per_day[day_idx].insert(block);
          }
        }
      }
    }
  }
  
  // For each day, determine which blocks to show
  // Rules:
  // 1) No intervals: Don't show any blocks (6 char minimum width, no time labels)
  // 2) At least one occupied block: Show at least 2 full 4-hour blocks (including the occupied one)
  // 3) More than 2 occupied blocks: Show each occupied block in full
  std::vector<std::set<int>> visible_blocks_per_day(actual_days);
  
  for (size_t day_idx = 0; day_idx < day_starts.size(); ++day_idx) {
    const auto& blocks_with_intervals = blocks_with_intervals_per_day[day_idx];
    
    if (blocks_with_intervals.empty()) {
      // Rule 1: No intervals - don't show any blocks (will use 6 char minimum width, no time labels)
      // visible_blocks_per_day[day_idx] remains empty
    } else {
      int num_occupied = blocks_with_intervals.size();
      int min_block = *blocks_with_intervals.begin();
      int max_block = *blocks_with_intervals.rbegin();
      int block_span = max_block - min_block + 1;
      
      // Check if blocks are consecutive (likely a calculation error for short intervals)
      bool all_consecutive = (num_occupied == block_span);
      
      // If we have 1-2 blocks, or if we have 3 consecutive blocks starting at 0 (likely error),
      // treat as rule 2
      if (num_occupied <= 2 || (all_consecutive && min_block == 0 && block_span <= 3)) {
        // Rule 2: At least one occupied block (1 or 2 blocks) - show at least 2 full blocks
        // Use only the first occupied block (likely the correct one) plus one adjacent
        int first_occupied = *blocks_with_intervals.begin();
        visible_blocks_per_day[day_idx].insert(first_occupied);
        // Add one adjacent block to make 2 total
        if (first_occupied > 0) {
          visible_blocks_per_day[day_idx].insert(first_occupied - 1);
        } else if (first_occupied < NUM_BLOCKS - 1) {
          visible_blocks_per_day[day_idx].insert(first_occupied + 1);
        }
      } else {
        // Rule 3: More than 2 occupied blocks - show each occupied block in full
        // Include all blocks from min to max (inclusive)
        for (int block = min_block; block <= max_block; ++block) {
          visible_blocks_per_day[day_idx].insert(block);
        }
      }
    }
  }
  
  // Calculate width per day based on maximum blocks needed across all tasks
  // First, calculate per-task blocks for each day to find the maximum needed
  std::vector<std::map<std::string, std::set<int>>> task_blocks_per_day(actual_days);
  for (const auto& task_pair : intervals_by_task) {
    const std::string& task_uuid = task_pair.first;
    const std::vector<Interval>& task_intervals = task_pair.second;
    
    for (const auto& interval : task_intervals) {
      time_t interval_start = std::max(interval.start_time, visible_start);
      time_t interval_end = std::min(interval.is_open ? time(nullptr) : interval.end_time, visible_end);
      if (interval_end <= interval_start) continue;
      
      for (size_t day_idx = 0; day_idx < day_starts.size(); ++day_idx) {
        time_t day_start = day_starts[day_idx];
        time_t day_end = day_start + (24 * 3600);
        
        if (interval_end > day_start && interval_start < day_end) {
          time_t day_interval_start = std::max(interval_start, day_start);
          time_t day_interval_end = std::min(interval_end, day_end);
          
          Datetime day_start_dt(day_interval_start);
          Datetime day_end_dt(day_interval_end);
          
          int start_hour = day_start_dt.hour();
          int end_hour = day_end_dt.hour();
          if (day_interval_end > day_start + (end_hour * 3600)) {
            end_hour++;
          }
          
          for (int hour = start_hour; hour < end_hour; ++hour) {
            int block = hour / BLOCK_SIZE;
            if (block >= 0 && block < NUM_BLOCKS) {
              task_blocks_per_day[day_idx][task_uuid].insert(block);
            }
          }
        }
      }
    }
  }
  
  // Calculate width per day based on maximum blocks needed across all tasks
  // Minimum 6 characters per day, or max(6, blocks_needed * 4)
  // But if there are no intervals, use minimum 6 characters (not block-based width)
  std::vector<int> chars_per_day_per_day(actual_days);
  int total_timeline_width = 0;
  // Process all days - use day_starts.size() to ensure we process exactly what we're displaying
  // day_starts.size() should equal actual_days
  for (size_t day_idx = 0; day_idx < day_starts.size() && day_idx < (size_t)actual_days; ++day_idx) {
    const auto& blocks_with_intervals = blocks_with_intervals_per_day[day_idx];
    
    if (blocks_with_intervals.empty()) {
      // Rule 1: No intervals - use 6 character minimum width
      chars_per_day_per_day[day_idx] = 6;
    } else {
      // Calculate width based on visible blocks (which follow rules 2 and 3)
      const auto& visible_blocks = visible_blocks_per_day[day_idx];
      int num_visible_blocks = visible_blocks.size();
      
      if (num_visible_blocks == 0) {
        // Shouldn't happen, but fallback to 6
        chars_per_day_per_day[day_idx] = 6;
      } else {
        // Width = number of visible blocks * 4 (each block is 4 characters)
        chars_per_day_per_day[day_idx] = num_visible_blocks * BLOCK_SIZE;
      }
    }
    
    total_timeline_width += chars_per_day_per_day[day_idx];
    // Add gap between days (except after last day)
    if (day_idx < day_starts.size() - 1) {
      total_timeline_width += 1;  // One space gap
    }
  }
  int timeline_width = total_timeline_width;
  
  // Calculate total chart width for separator lines (label_width + timeline_width)
  int total_chart_width = label_width + timeline_width;
  
  // Format date range header (end_dt already declared above)
  Datetime visible_start_dt_header(visible_start);
  
  out << "Timeline View (" << visible_start_dt_header.toString("Y-M-D") << " to " << end_dt.toString("Y-M-D") << ")\n";
  out << std::string(total_chart_width, '=') << "\n";
  
  // Build date axis - show last 7 days (or fewer if range is shorter)
  // Use same label width as task labels for alignment
  std::string date_header = "Date:";
  int date_header_padding = label_width - utf8_width(date_header);
  if (date_header_padding > 0) {
    date_header += std::string(date_header_padding, ' ');
  }
  out << date_header;
  
  // Work forward chronologically from visible_start (day_starts is already in chronological order)
  for (size_t i = 0; i < day_starts.size(); ++i) {
    time_t day_start_epoch = day_starts[i];
    Datetime day_start_dt(day_start_epoch);
    // Format as "d-Mon" (e.g., "25-Dec")
    int day = day_start_dt.day();
    std::string month_abbr = Datetime::monthNameShort(day_start_dt.month());
    // Capitalize first letter
    if (month_abbr.length() > 0) {
      month_abbr[0] = toupper(month_abbr[0]);
    }
    std::string day_str = format("{1}-{2}", day, month_abbr);
    // Left-align dates to match time labels and bars
    // Use per-day width
    int day_width = chars_per_day_per_day[i];
    out << day_str;
    int padding = day_width - utf8_width(day_str);
    if (padding > 0) out << std::string(padding, ' ');
    // Add one space gap between dates (except after last date)
    if (i < day_starts.size() - 1) {
      out << " ";
    }
  }
  
  out << "\n";
  
  // Build time axis - show block labels for each day
  // Use same label width as task labels for alignment
  std::string time_header = "Time:";
  int time_header_padding = label_width - utf8_width(time_header);
  if (time_header_padding > 0) {
    time_header += std::string(time_header_padding, ' ');
  }
  out << time_header;
  
  for (size_t day_idx = 0; day_idx < day_starts.size(); ++day_idx) {
    const auto& visible_blocks = visible_blocks_per_day[day_idx];
    const auto& blocks_with_intervals = blocks_with_intervals_per_day[day_idx];
    int day_width = chars_per_day_per_day[day_idx];
    
    if (blocks_with_intervals.empty()) {
      // Rule 1: No intervals - no time labels, just spaces (6 characters)
      out << std::string(day_width, ' ');
    } else {
      // Rules 2 & 3: Show labels for all visible blocks
      // Each block is 4 hours, so 4 characters
      // Format: "HH  " (2 chars for hour + 2 spaces = 4 chars total)
      // Sort visible blocks to maintain order
      std::vector<int> sorted_blocks(visible_blocks.begin(), visible_blocks.end());
      std::sort(sorted_blocks.begin(), sorted_blocks.end());
      
      int chars_used = 0;
      for (int block : sorted_blocks) {
        // Show label for all visible blocks (not just blocks with intervals)
        int block_start_hour = block * BLOCK_SIZE;
        char hour_str[3];
        snprintf(hour_str, 3, "%02d", block_start_hour);
        out << hour_str << "  ";  // 2 chars for hour + 2 spaces = 4 chars total
        chars_used += 4;
      }
      
      // Pad to day_width (should match exactly, but safety check)
      if (chars_used < day_width) {
        out << std::string(day_width - chars_used, ' ');
      }
    }
    
    // Add one space gap between days (except after last day)
    if (day_idx < day_starts.size() - 1) {
      out << " ";
    }
  }
  out << "\n";
  out << std::string(total_chart_width, '-') << "\n";
  
  // For each project, show tasks
  for (const auto& project_pair : tasks_by_project) {
    const std::string& project = project_pair.first;
    const std::vector<Task>& project_tasks = project_pair.second;
    
    // Show project header
    out << project << "\n";
    
    // For each task in project, show timeline
    for (const auto& task : project_tasks) {
      std::string task_uuid = task.get("uuid");
      
      if (intervals_by_task.find(task_uuid) == intervals_by_task.end()) {
        continue;  // Skip tasks with no intervals
      }
      
      const std::vector<Interval>& intervals = intervals_by_task[task_uuid];
      
      // Build task label: "<number>: <description snippet>"
      // Right-align task ID with padding based on max_task_id
      std::string task_id_str;
      if (task.id != 0) {
        // Right-align numeric ID with padding
        char id_buf[32];
        snprintf(id_buf, sizeof(id_buf), "%*d", id_digits, task.id);
        task_id_str = id_buf;
      } else {
        // For completed/deleted tasks, use "-" as placeholder
        task_id_str = "-";
        // Right-align "-" to match ID width
        if (1 < id_digits) {
          task_id_str = std::string(id_digits - 1, ' ') + task_id_str;
        }
      }
      
      std::string description = task.get("description");
      
      // Truncate description to fit (similar to intervals table)
      std::string short_desc;
      int desc_width = utf8_width(description);
      if (desc_width > 20) {
        // Truncate to fit exactly 20 characters: use 19 chars + 1 char for ellipsis
        short_desc = utf8_substr(description, 0, 19) + "…";
        int short_width = utf8_width(short_desc);
        if (short_width != 20) {
          int ellipsis_width = utf8_width("…");
          int target_length = 20 - ellipsis_width;
          short_desc = utf8_substr(description, 0, target_length) + "…";
        }
      } else {
        short_desc = description;
      }
      
      std::string label = format("{1}: {2}", task_id_str, short_desc);
      if (utf8_width(label) < label_width) {
        label += std::string(label_width - utf8_width(label), ' ');
      }
      // Indent task by one space, trim one space from end to keep total width
      label = " " + label;
      // Remove one space from the end if possible
      if (label.length() > 0 && label[label.length() - 1] == ' ') {
        label = label.substr(0, label.length() - 1);
      }
      
      // Build timeline bar using 4-hour blocks
      // Use ░ for inactive, █ for active (matching the plan)
      const char* active_char = "\xe2\x96\x88";  // █ (U+2588) in UTF-8
      const char* inactive_char = "\xe2\x96\x91";  // ░ (U+2591) in UTF-8
      
      std::string timeline_bar;
      
      // For each day, render the visible blocks
      for (size_t day_idx = 0; day_idx < day_starts.size(); ++day_idx) {
        time_t day_start = day_starts[day_idx];
        const auto& visible_blocks = visible_blocks_per_day[day_idx];
        const auto& blocks_with_intervals = blocks_with_intervals_per_day[day_idx];
        int day_width = chars_per_day_per_day[day_idx];
        
        // If no intervals, just render minimum width (6 chars) of inactive characters
        if (blocks_with_intervals.empty()) {
          for (int i = 0; i < day_width; ++i) {
            timeline_bar += inactive_char;
          }
        } else {
          // Create a map of block -> has_interval for this day
          std::map<int, bool> block_has_interval;
          for (int block : visible_blocks) {
            block_has_interval[block] = false;
          }
          
          // Check if any intervals overlap with each visible block
          for (const auto& interval : intervals) {
            time_t interval_start = std::max(interval.start_time, visible_start);
            time_t interval_end = std::min(interval.is_open ? time(nullptr) : interval.end_time, visible_end);
            
            if (interval_end <= interval_start) continue;
            
            // Check if interval overlaps with this day
            time_t day_end = day_start + (24 * 3600);
            if (interval_end > day_start && interval_start < day_end) {
              // Calculate which blocks this interval touches
              time_t day_interval_start = std::max(interval_start, day_start);
              time_t day_interval_end = std::min(interval_end, day_end);
              
              Datetime day_start_dt(day_interval_start);
              Datetime day_end_dt(day_interval_end);
              
              int start_hour = day_start_dt.hour();
              int end_hour = day_end_dt.hour();
              if (day_interval_end > day_start + (end_hour * 3600)) {
                end_hour++;
              }
              
              // Mark blocks that are touched
              for (int hour = start_hour; hour < end_hour; ++hour) {
                int block = hour / BLOCK_SIZE;
                if (block_has_interval.find(block) != block_has_interval.end()) {
                  block_has_interval[block] = true;
                }
              }
            }
          }
          
          // Render blocks for this day - render all visible blocks, showing this task's intervals
          // Sort visible blocks to maintain order
          std::vector<int> sorted_visible_blocks(visible_blocks.begin(), visible_blocks.end());
          std::sort(sorted_visible_blocks.begin(), sorted_visible_blocks.end());
          
          int chars_used = 0;
          // Render all visible blocks (each block is 4 characters)
          for (int block : sorted_visible_blocks) {
            if (chars_used >= day_width) break;  // Stop if we've reached the day width
            // Check if this task has intervals in this block
            bool has_interval = block_has_interval[block];
            // Each block is 4 hours = 4 characters
            int remaining = day_width - chars_used;
            int chars_to_render = std::min(BLOCK_SIZE, remaining);
            for (int i = 0; i < chars_to_render; ++i) {
              timeline_bar += has_interval ? active_char : inactive_char;
            }
            chars_used += chars_to_render;
          }
          
          // Pad to day_width (should not be needed if logic above is correct, but safety check)
          if (chars_used < day_width) {
            for (int i = 0; i < day_width - chars_used; ++i) {
              timeline_bar += inactive_char;
            }
          }
        }
        
        // Add one space gap between days (except after last day)
        if (day_idx < day_starts.size() - 1) {
          timeline_bar += " ";
        }
      }
      
      // Calculate expected width (timeline_width already includes gaps between days)
      int expected_width = timeline_width;  // Already includes gaps between days
      
      // Ensure exact width
      while ((int)utf8_width(timeline_bar) > expected_width) {
        if (timeline_bar.length() >= 3) {
          timeline_bar = timeline_bar.substr(0, timeline_bar.length() - 3);
        } else {
          break;
        }
      }
      while ((int)utf8_width(timeline_bar) < expected_width) {
        timeline_bar += inactive_char;
      }
      
      out << label << timeline_bar << "\n";
    }
    
    out << "\n";
  }
  
  out << std::string(total_chart_width, '-') << "\n";
  out << "Legend: █ = Active work  ░ = No work\n";
  
  // Calculate and show total
  time_t total_time = 0;
  for (const auto& interval : all_intervals) {
    time_t interval_start = std::max(interval.start_time, start_time);
    time_t interval_end = std::min(interval.is_open ? time(nullptr) : interval.end_time, end_time);
    if (interval_end > interval_start) {
      total_time += (interval_end - interval_start);
    }
  }
  
  Duration total_dur(total_time);
  out << "Total: " << total_dur.format() << "\n";
  
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string ChartRenderer::renderCumulative(
  const std::map<time_t, time_t>& cumulative_data
) {
  if (cumulative_data.empty()) {
    return "No data to display.\n";
  }

  std::stringstream out;
  out << "Cumulative Work Time\n";
  out << std::string(60, '=') << "\n";
  out << "Date       Daily    Cumulative\n";
  out << std::string(60, '-') << "\n";

  time_t previous_cumulative = 0;
  for (const auto& pair : cumulative_data) {
    Datetime dt(pair.first);
    time_t daily = pair.second - previous_cumulative;
    previous_cumulative = pair.second;
    
    out << dt.toString("Y-M-D") << "  "
        << formatDuration(daily) << "  "
        << formatDuration(pair.second) << "\n";
  }

  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
