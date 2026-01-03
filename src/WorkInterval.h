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

#ifndef INCLUDED_WORKINTERVAL
#define INCLUDED_WORKINTERVAL

#include <cmake.h>
// cmake.h include header must come first

#include <string>
#include <vector>
#include <time.h>

// Represents a single work interval (start -> stop/done)
struct Interval {
  std::string task_uuid;
  time_t start_time;
  time_t end_time;
  std::string event_type;  // "stop" or "done"
  int interval_id;
  bool is_open;  // true if interval is still open (no stop/done event yet)
  // Note: Messages are stored as annotations with the interval timestamp, not here
  
  // Calculate duration in seconds
  // For open intervals, calculates from start to current time
  time_t duration() const {
    if (is_open) {
      return time(nullptr) - start_time;
    }
    return end_time - start_time;
  }
};

// WorkInterval class manages work interval tracking in SQLite database
class WorkInterval {
 public:
  // Initialize the work intervals table in the database
  // Should be called once at startup with the database path
  static void initialize(const std::string& db_path);

  // Log a work interval event (start, stop, or done)
  // Messages are stored as annotations with the interval timestamp, not in work_intervals
  static void log_event(const std::string& task_uuid,
                       const std::string& event_type,  // "start", "stop", or "done"
                       time_t timestamp);

  // Get all intervals for a specific task (by UUID)
  static std::vector<Interval> get_intervals(const std::string& task_uuid);

  // Get all intervals within a date range
  static std::vector<Interval> get_intervals_by_date(time_t start_time,
                                                      time_t end_time);

  // Get the next interval_id for a task (used to link start/stop/done events)
  static int get_next_interval_id(const std::string& task_uuid);

  // Modify an interval's start or stop time
  // attribute: "start", "stop", or "end" (end is alias for stop)
  static void modify_interval(const std::string& task_uuid,
                               int interval_id,
                               const std::string& attribute,
                               time_t new_timestamp,
                               int task_id = 0);  // Optional task ID for error messages

  // Fill an interval to meet neighbors (task-agnostic)
  // fill_type: "start", "stop", or "both"
  static void fill_interval(const std::string& task_uuid,
                           int interval_id,
                           const std::string& fill_type);

 private:
  // Ensure the table exists (lazy initialization)
  static void ensure_table_exists();

  // Get the database path (from Context)
  static std::string get_db_path();

  // Check if table exists
  static bool table_exists();

  // Create the work_intervals table
  static void create_table();

  static bool _initialized;
  static std::string _db_path;
};

#endif
////////////////////////////////////////////////////////////////////////////////
