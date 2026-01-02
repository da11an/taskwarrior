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

#include <WorkInterval.h>
#include <Context.h>
#include <FS.h>
#include <format.h>
#include <shared.h>
#include <taskchampion-cpp/lib.h>

#include <cstring>
#include <sstream>

// Static member initialization
bool WorkInterval::_initialized = false;
std::string WorkInterval::_db_path = "";

////////////////////////////////////////////////////////////////////////////////
std::string WorkInterval::get_db_path() {
  if (!_db_path.empty()) {
    return _db_path;
  }

  // Get database directory from Context (same as TaskChampion uses)
  std::string data_location = Context::getContext().data_dir._data;
  _db_path = data_location;  // Store directory, not full path
  return _db_path;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::initialize(const std::string& db_path) {
  _db_path = db_path;
  ensure_table_exists();
  _initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::ensure_table_exists() {
  std::string taskdb_dir = get_db_path();
  try {
    tc::work_interval_initialize(taskdb_dir);
  } catch (const std::exception& e) {
    std::string error = format("Cannot initialize work intervals table: {1}", e.what());
    throw error;
  }
}

////////////////////////////////////////////////////////////////////////////////
int WorkInterval::get_next_interval_id(const std::string& task_uuid) {
  // This method is no longer needed - interval_id is determined in Rust
  // Keeping for API compatibility but it's not used
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::log_event(const std::string& task_uuid,
                             const std::string& event_type,
                             time_t timestamp) {
  ensure_table_exists();

  std::string taskdb_dir = get_db_path();
  try {
    tc::work_interval_log_event(taskdb_dir, task_uuid, event_type, static_cast<int64_t>(timestamp));
  } catch (const std::exception& e) {
    std::string error = format("Cannot log work interval: {1}", e.what());
    throw error;
  }
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Interval> WorkInterval::get_intervals(const std::string& task_uuid) {
  ensure_table_exists();

  std::vector<Interval> intervals;
  std::string taskdb_dir = get_db_path();
  
  try {
    rust::Vec<tc::WorkInterval> rust_intervals = tc::work_interval_get_intervals(taskdb_dir, task_uuid);
    
    for (size_t i = 0; i < rust_intervals.size(); ++i) {
      const auto& ri = rust_intervals[i];
      Interval interval;
      interval.task_uuid = std::string(ri.task_uuid.data(), ri.task_uuid.size());
      interval.start_time = static_cast<time_t>(ri.start_time);
      interval.end_time = static_cast<time_t>(ri.end_time);
      interval.event_type = std::string(ri.event_type.data(), ri.event_type.size());
      interval.interval_id = ri.interval_id;
      interval.is_open = ri.is_open;
      intervals.push_back(interval);
    }
  } catch (const std::exception& e) {
    // Return empty on error
    return intervals;
  }
  
  return intervals;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Interval> WorkInterval::get_intervals_by_date(time_t start_time,
                                                          time_t end_time) {
  ensure_table_exists();

  std::vector<Interval> intervals;
  std::string taskdb_dir = get_db_path();
  
  try {
    rust::Vec<tc::WorkInterval> rust_intervals = tc::work_interval_get_intervals_by_date(
        taskdb_dir, static_cast<int64_t>(start_time), static_cast<int64_t>(end_time));
    
    for (size_t i = 0; i < rust_intervals.size(); ++i) {
      const auto& ri = rust_intervals[i];
      Interval interval;
      interval.task_uuid = std::string(ri.task_uuid.data(), ri.task_uuid.size());
      interval.start_time = static_cast<time_t>(ri.start_time);
      interval.end_time = static_cast<time_t>(ri.end_time);
      interval.event_type = std::string(ri.event_type.data(), ri.event_type.size());
      interval.interval_id = ri.interval_id;
      interval.is_open = ri.is_open;
      intervals.push_back(interval);
    }
  } catch (const std::exception& e) {
    // Return empty on error
    return intervals;
  }
  
  return intervals;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::modify_interval(const std::string& task_uuid,
                                   int interval_id,
                                   const std::string& attribute,
                                   time_t new_timestamp) {
  ensure_table_exists();

  // Validate attribute
  std::string event_type;
  if (attribute == "start") {
    event_type = "start";
  } else if (attribute == "stop" || attribute == "end") {
    event_type = "stop";  // Rust function handles both stop and done
  } else {
    throw std::string(format("Invalid attribute '{1}'. Must be 'start', 'stop', or 'end'.", attribute));
  }

  // Get the interval to validate it exists and check for overlaps
  std::vector<Interval> intervals = get_intervals(task_uuid);
  Interval* target_interval = nullptr;
  for (auto& interval : intervals) {
    if (interval.interval_id == interval_id) {
      target_interval = &interval;
      break;
    }
  }

  if (!target_interval) {
    throw std::string(format("Interval with ID {1} not found for task {2}.", interval_id, task_uuid));
  }

  // Validate new timestamp doesn't create overlaps
  time_t new_start = target_interval->start_time;
  time_t new_end = target_interval->end_time;
  bool is_open = target_interval->is_open;

  if (event_type == "start") {
    new_start = new_timestamp;
    // Check overlap with previous interval
    for (const auto& interval : intervals) {
      if (interval.interval_id != interval_id && !interval.is_open && interval.end_time > new_start) {
        if (interval.start_time < new_start) {
          throw std::string(format("Cannot modify interval: would create overlap with interval {1}.{2}",
                                   task_uuid, interval.interval_id));
        }
      }
    }
    // Validate start < end (if not open)
    if (!is_open && new_start >= new_end) {
      throw std::string("Cannot modify interval: start time must be before stop time.");
    }
  } else {
    // stop/end
    new_end = new_timestamp;
    // If modifying stop time of open interval, it becomes closed
    is_open = false;
    // Check overlap with next interval
    for (const auto& interval : intervals) {
      if (interval.interval_id != interval_id && interval.start_time < new_end) {
        if (!interval.is_open || interval.start_time > new_start) {
          throw std::string(format("Cannot modify interval: would create overlap with interval {1}.{2}",
                                   task_uuid, interval.interval_id));
        }
      }
    }
    // Validate start < end
    if (new_start >= new_end) {
      throw std::string("Cannot modify interval: start time must be before stop time.");
    }
  }

  // Update the timestamp via Rust bridge
  std::string taskdb_dir = get_db_path();
  try {
    tc::work_interval_update_timestamp(taskdb_dir, task_uuid, interval_id, event_type,
                                      static_cast<int64_t>(new_timestamp));
  } catch (const std::exception& e) {
    std::string error = format("Cannot update interval timestamp: {1}", e.what());
    throw error;
  }
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::fill_interval(const std::string& task_uuid,
                                 int interval_id,
                                 const std::string& fill_type) {
  ensure_table_exists();

  std::string taskdb_dir = get_db_path();
  try {
    tc::work_interval_fill(taskdb_dir, task_uuid, interval_id, fill_type);
  } catch (const std::exception& e) {
    std::string error = format("Cannot fill interval: {1}", e.what());
    throw error;
  }
}

////////////////////////////////////////////////////////////////////////////////