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
#include <IntervalAggregator.h>
#include <Task.h>
#include <WorkInterval.h>
#include <Datetime.h>

#include <algorithm>
#include <map>

////////////////////////////////////////////////////////////////////////////////
std::map<std::string, time_t> IntervalAggregator::aggregateByProject(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::map<std::string, time_t> result;

  for (const auto& task : tasks) {
    std::string project = task.get("project");
    if (project == "") project = "Uncategorized";

    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    for (const auto& interval : intervals) {
      // Check if interval overlaps with date range
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;

      // Skip if interval is completely outside range
      if (interval_end < range.start || interval_start > range.end) {
        continue;
      }

      // Calculate overlap duration
      time_t overlap_start = std::max(interval_start, range.start);
      time_t overlap_end = std::min(interval_end, range.end);
      time_t overlap_duration = overlap_end - overlap_start;

      if (overlap_duration > 0) {
        result[project] += overlap_duration;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
std::map<int, time_t> IntervalAggregator::aggregateByTask(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::map<int, time_t> result;

  for (const auto& task : tasks) {
    int task_id = task.id;
    if (task_id == 0) continue;  // Skip tasks without IDs

    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    for (const auto& interval : intervals) {
      // Check if interval overlaps with date range
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;

      // Skip if interval is completely outside range
      if (interval_end < range.start || interval_start > range.end) {
        continue;
      }

      // Calculate overlap duration
      time_t overlap_start = std::max(interval_start, range.start);
      time_t overlap_end = std::min(interval_end, range.end);
      time_t overlap_duration = overlap_end - overlap_start;

      if (overlap_duration > 0) {
        result[task_id] += overlap_duration;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
std::map<int, time_t> IntervalAggregator::aggregateByDayOfWeek(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::map<int, time_t> result;

  for (const auto& task : tasks) {
    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    for (const auto& interval : intervals) {
      // Check if interval overlaps with date range
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;

      // Skip if interval is completely outside range
      if (interval_end < range.start || interval_start > range.end) {
        continue;
      }

      // Process each day in the interval
      time_t current = std::max(interval_start, range.start);
      time_t end = std::min(interval_end, range.end);

      while (current < end) {
        Datetime dt(current);
        int day_of_week = dt.dayOfWeek();  // 0=Sunday, 6=Saturday

        // Calculate duration for this day
        Datetime day_start(dt.year(), dt.month(), dt.day(), 0, 0, 0);
        Datetime day_end(dt.year(), dt.month(), dt.day(), 23, 59, 59);
        time_t day_start_epoch = day_start.toEpoch();
        time_t day_end_epoch = day_end.toEpoch();

        time_t day_overlap_start = std::max(current, day_start_epoch);
        time_t day_overlap_end = std::min(end, day_end_epoch);
        time_t day_duration = day_overlap_end - day_overlap_start;

        if (day_duration > 0) {
          result[day_of_week] += day_duration;
        }

        // Move to next day
        current = day_end_epoch + 1;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
std::map<time_t, time_t> IntervalAggregator::aggregateByDate(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::map<time_t, time_t> result;

  for (const auto& task : tasks) {
    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    for (const auto& interval : intervals) {
      // Check if interval overlaps with date range
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;

      // Skip if interval is completely outside range
      if (interval_end < range.start || interval_start > range.end) {
        continue;
      }

      // Process each day in the interval
      time_t current = std::max(interval_start, range.start);
      time_t end = std::min(interval_end, range.end);

      while (current < end) {
        Datetime dt(current);
        
        // Get start of day (midnight)
        Datetime day_start(dt.year(), dt.month(), dt.day(), 0, 0, 0);
        time_t day_epoch = day_start.toEpoch();

        // Calculate duration for this day
        Datetime day_end(dt.year(), dt.month(), dt.day(), 23, 59, 59);
        time_t day_end_epoch = day_end.toEpoch();

        time_t day_overlap_start = std::max(current, day_epoch);
        time_t day_overlap_end = std::min(end, day_end_epoch);
        time_t day_duration = day_overlap_end - day_overlap_start;

        if (day_duration > 0) {
          result[day_epoch] += day_duration;
        }

        // Move to next day
        current = day_end_epoch + 1;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Interval> IntervalAggregator::getAllIntervals(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::vector<Interval> result;

  for (const auto& task : tasks) {
    std::string task_uuid = task.get("uuid");
    std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);

    for (const auto& interval : intervals) {
      // Check if interval overlaps with date range
      time_t interval_start = interval.start_time;
      time_t interval_end = interval.is_open ? time(nullptr) : interval.end_time;

      if (interval_end >= range.start && interval_start <= range.end) {
        result.push_back(interval);
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
std::map<time_t, time_t> IntervalAggregator::calculateCumulative(
  const std::vector<Task>& tasks,
  const DateRange& range
) {
  std::map<time_t, time_t> daily = aggregateByDate(tasks, range);
  std::map<time_t, time_t> cumulative;
  
  time_t running_total = 0;
  
  // Sort by date
  std::vector<time_t> dates;
  for (const auto& pair : daily) {
    dates.push_back(pair.first);
  }
  std::sort(dates.begin(), dates.end());
  
  for (time_t date : dates) {
    running_total += daily[date];
    cumulative[date] = running_total;
  }
  
  return cumulative;
}

////////////////////////////////////////////////////////////////////////////////
