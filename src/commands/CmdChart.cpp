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
#include <CmdChart.h>
#include <Context.h>
#include <Datetime.h>
#include <Duration.h>
#include <Filter.h>
#include <IntervalAggregator.h>
#include <ChartRenderer.h>
#include <format.h>
#include <shared.h>

#include <algorithm>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
CmdChart::CmdChart() {
  _keyword = "chart";
  _usage = "task [<filter>] chart [<type>]";
  _description = "Shows graphical visualizations of work intervals";
  _read_only = true;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = true;
  _accepts_filter = true;
  _accepts_modifications = false;
  _accepts_miscellaneous = true;  // Allow type argument
  _category = Command::Category::graphs;
}

////////////////////////////////////////////////////////////////////////////////
int CmdChart::execute(std::string& output) {
  int rc = 0;

  // Apply filter
  Filter filter;
  std::vector<Task> filtered;
  filter.subset(filtered);

  if (filtered.size() == 0) {
    Context::getContext().footnote("No tasks specified.");
    return 1;
  }

  // Get chart type from command line arguments
  std::string chart_type = "timeline";  // Default
  
  // Get miscellaneous arguments (words after the command)
  auto misc = Context::getContext().cli2.getMiscellaneous();
  if (!misc.empty()) {
    // First miscellaneous argument is the chart type
    chart_type = misc[0].attribute("raw");
  }
  
  // Also check words as fallback
  if (chart_type == "timeline") {
    auto words = Context::getContext().cli2.getWords();
    for (size_t i = 0; i < words.size(); ++i) {
      if (words[i] == "chart" && i + 1 < words.size()) {
        chart_type = words[i + 1];
        break;
      }
    }
  }

  // Determine date range (default: last 30 days)
  Datetime now;
  time_t end_time = now.toEpoch();
  time_t start_time = end_time - (30 * 24 * 60 * 60);  // 30 days ago

  DateRange range;
  range.start = start_time;
  range.end = end_time;

  std::stringstream out;

  // Route to appropriate visualization
  if (chart_type == "project" || chart_type == "projects") {
    // Project time allocation
    auto project_data = IntervalAggregator::aggregateByProject(filtered, range);
    
    std::vector<BarData> bars;
    for (const auto& pair : project_data) {
      BarData bar;
      bar.label = pair.first;
      bar.value = pair.second;
      bar.color_key = pair.first;
      bars.push_back(bar);
    }
    
    // Sort by value (descending)
    std::sort(bars.begin(), bars.end(),
              [](const BarData& a, const BarData& b) {
                return a.value > b.value;
              });
    
    int chart_width = Context::getContext().getWidth();
    out << "Project Time Allocation (Last 30 Days)\n";
    out << std::string(chart_width, '=') << "\n";
    out << ChartRenderer::renderBarChart(bars, chart_width, true);
    
  } else if (chart_type == "task" || chart_type == "tasks") {
    // Task time allocation
    auto task_data = IntervalAggregator::aggregateByTask(filtered, range);
    
    std::vector<BarData> bars;
    for (const auto& pair : task_data) {
      // Find task description
      std::string description = "Task " + format("{1}", pair.first);
      for (const auto& task : filtered) {
        if (task.id == pair.first) {
          description = task.get("description");
          if (utf8_width(description) > 40) {
            description = utf8_substr(description, 0, 40) + "...";
          }
          break;
        }
      }
      
      BarData bar;
      bar.label = format("#{1} {2}", pair.first, description);
      bar.value = pair.second;
      bar.color_key = format("task{1}", pair.first);
      bars.push_back(bar);
    }
    
    // Sort by value (descending)
    std::sort(bars.begin(), bars.end(),
              [](const BarData& a, const BarData& b) {
                return a.value > b.value;
              });
    
    // Limit to top 20
    if (bars.size() > 20) {
      bars.resize(20);
    }
    
    out << "Top Tasks by Time (Last 30 Days)\n";
    out << ChartRenderer::renderBarChart(bars, Context::getContext().getWidth(), false);
    
  } else if (chart_type == "daily") {
    // Daily breakdown by day of week
    auto daily_data = IntervalAggregator::aggregateByDayOfWeek(filtered, range);
    
    const char* day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                                "Thursday", "Friday", "Saturday"};
    
    std::vector<BarData> bars;
    for (int dow = 0; dow < 7; ++dow) {
      time_t value = 0;
      if (daily_data.find(dow) != daily_data.end()) {
        value = daily_data[dow];
      }
      
      BarData bar;
      bar.label = day_names[dow];
      bar.value = value;
      bar.color_key = format("dow{1}", dow);
      bars.push_back(bar);
    }
    
    out << "Daily Work Pattern (Last 30 Days)\n";
    out << ChartRenderer::renderBarChart(bars, Context::getContext().getWidth(), false);
    
    // Calculate totals
    time_t total = 0;
    for (const auto& bar : bars) {
      total += bar.value;
    }
    Duration total_dur(total);
    out << "\nTotal: " << total_dur.format() << "\n";
    if (total > 0) {
      Duration avg_dur(total / 7);
      out << "Average per day: " << avg_dur.format() << "\n";
    }
    
  } else if (chart_type == "calendar") {
    // Calendar heatmap - use 12 weeks range
    DateRange calendar_range;
    time_t end_time = time(nullptr);
    time_t start_time = end_time - (12 * 7 * 24 * 60 * 60);  // 12 weeks ago
    calendar_range.start = start_time;
    calendar_range.end = end_time;
    auto daily_data = IntervalAggregator::aggregateByDate(filtered, calendar_range);
    out << ChartRenderer::renderCalendarHeatmap(daily_data, 12);
    
  } else if (chart_type == "cumulative") {
    // Cumulative time view
    auto cumulative_data = IntervalAggregator::calculateCumulative(filtered, range);
    out << ChartRenderer::renderCumulative(cumulative_data);
    
  } else if (chart_type == "timeline" || chart_type == "") {
    // Timeline view (default)
    out << ChartRenderer::renderTimeline(filtered, start_time, end_time, "hour");
    
  } else {
    // Unknown chart type
    out << "Unknown chart type: " << chart_type << "\n";
    out << "Available types: timeline, project, task, daily, calendar, cumulative\n";
    return 1;
  }

  output = out.str();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
