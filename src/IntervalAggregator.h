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

#ifndef INCLUDED_INTERVALAGGREGATOR
#define INCLUDED_INTERVALAGGREGATOR

#include <WorkInterval.h>

#include <map>
#include <string>
#include <vector>

class Task;

struct DateRange {
  time_t start;
  time_t end;
};

class IntervalAggregator {
 public:
  // Aggregate intervals by project
  static std::map<std::string, time_t> aggregateByProject(
    const std::vector<Task>& tasks,
    const DateRange& range
  );

  // Aggregate intervals by task ID
  static std::map<int, time_t> aggregateByTask(
    const std::vector<Task>& tasks,
    const DateRange& range
  );

  // Aggregate intervals by day of week (0=Sunday, 6=Saturday)
  static std::map<int, time_t> aggregateByDayOfWeek(
    const std::vector<Task>& tasks,
    const DateRange& range
  );

  // Aggregate intervals by date (day)
  static std::map<time_t, time_t> aggregateByDate(
    const std::vector<Task>& tasks,
    const DateRange& range
  );

  // Get all intervals for tasks within date range
  static std::vector<Interval> getAllIntervals(
    const std::vector<Task>& tasks,
    const DateRange& range
  );

  // Calculate cumulative time over period
  static std::map<time_t, time_t> calculateCumulative(
    const std::vector<Task>& tasks,
    const DateRange& range
  );
};

#endif
////////////////////////////////////////////////////////////////////////////////
