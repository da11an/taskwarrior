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

#ifndef INCLUDED_CHARTRENDERER
#define INCLUDED_CHARTRENDERER

#include <Color.h>
#include <Duration.h>
#include <format.h>
#include <utf8.h>

#include <map>
#include <string>
#include <vector>

struct BarData {
  std::string label;
  time_t value;
  std::string color_key;  // For consistent color assignment
};

class ChartRenderer {
 public:
  // Render a horizontal bar chart
  static std::string renderBarChart(
    const std::vector<BarData>& data,
    int width,
    bool show_percentages = false
  );

  // Render a calendar heatmap
  static std::string renderCalendarHeatmap(
    const std::map<time_t, time_t>& daily_data,
    int weeks = 12
  );

  // Render a timeline view
  static std::string renderTimeline(
    const std::vector<class Task>& tasks,
    time_t start_time,
    time_t end_time,
    const std::string& granularity = "hour"
  );

  // Render cumulative line chart (ASCII)
  static std::string renderCumulative(
    const std::map<time_t, time_t>& cumulative_data
  );

  // Get color for project/task (consistent color assignment)
  static Color getColorForItem(const std::string& identifier);

 private:
  // Helper to get block character for intensity
  static char getBlockChar(double intensity);
  
  // Helper to format duration
  static std::string formatDuration(time_t seconds);
};

#endif
////////////////////////////////////////////////////////////////////////////////
