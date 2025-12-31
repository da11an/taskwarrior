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

#include <ColWorkDuration.h>
#include <Context.h>
#include <Duration.h>
#include <WorkInterval.h>
#include <format.h>
#include <utf8.h>

////////////////////////////////////////////////////////////////////////////////
ColumnWorkDuration::ColumnWorkDuration() {
  _name = "workduration";
  _label = "Duration";
  _modifiable = false;
  _type = "duration";
  _styles = {"default"};
  _examples = {"0:15:30"};
}

////////////////////////////////////////////////////////////////////////////////
// Set the minimum and maximum widths for the value.
void ColumnWorkDuration::measure(Task& task, unsigned int& minimum, unsigned int& maximum) {
  std::string task_uuid = task.get("uuid");
  std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);
  
  time_t total_duration = 0;
  for (const auto& interval : intervals) {
    total_duration += interval.duration();
  }
  
  Duration duration(total_duration);
  std::string formatted = duration.format();
  minimum = maximum = utf8_width(formatted);
}

////////////////////////////////////////////////////////////////////////////////
void ColumnWorkDuration::render(std::vector<std::string>& lines, Task& task, int width, Color& color) {
  std::string task_uuid = task.get("uuid");
  std::vector<Interval> intervals = WorkInterval::get_intervals(task_uuid);
  
  time_t total_duration = 0;
  for (const auto& interval : intervals) {
    total_duration += interval.duration();
  }
  
  Duration duration(total_duration);
  std::string formatted = duration.format();
  
  renderStringRight(lines, width, color, formatted);
}

////////////////////////////////////////////////////////////////////////////////
