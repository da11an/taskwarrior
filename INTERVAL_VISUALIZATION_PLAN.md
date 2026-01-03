# Work Interval Visualization Plan

## Overview
Add graphical command-line visualizations for work intervals to help users understand time allocation patterns, productivity trends, and project management insights. These visualizations will be consistent with Taskwarrior's existing syntax and color system.

## Existing Visualization Features (Avoid Duplication)

Taskwarrior already has several visualization commands that we should **not duplicate**:

1. **Burndown Charts** (`burndown.daily`, `burndown.weekly`, `burndown.monthly`):
   - Shows task **counts** over time (pending, started, done)
   - Tracks task lifecycle (entry → start → completion)
   - Uses ASCII art with PP (Pending), SS (Started), DD (Done)
   - Calculates net fix rate and estimated completion
   - **Focus**: Task status counts, not time spent

2. **History Charts** (`ghistory.daily`, `ghistory.weekly`, `ghistory.monthly`, `ghistory.annual`):
   - Shows task history (added, completed, deleted) over time
   - Bar charts showing **counts** of tasks added/completed/deleted
   - **Focus**: Task creation/completion counts, not time spent

3. **Calendar** (`calendar`):
   - Shows monthly calendar with **due tasks** marked
   - Can show multiple months
   - Marks dates with due tasks
   - **Focus**: Due dates, not work intervals

4. **Timesheet** (`timesheet`):
   - Shows weekly report of **completed and started tasks**
   - Table format listing tasks
   - **Focus**: Task events (started/completed), not time spent

**Key Distinction**: All existing visualizations focus on **task counts** and **task events**, not on **time spent** or **work intervals**. Our new visualizations will focus exclusively on **time allocation** and **work intervals**, which is a completely different dimension of data.

## Design Principles

1. **Consistency with Taskwarrior Syntax**: Follow existing patterns (`task <filter> <command> [options]`)
2. **Color-Aware**: Leverage Taskwarrior's color system (16, 256, 24-bit) with graceful degradation
3. **Terminal-Friendly**: Use Unicode box-drawing characters and block characters for graphics
4. **Filterable**: All visualizations respect Taskwarrior filters (project, tags, date ranges, etc.)
5. **Read-Only**: Visualizations are informational only, no modifications
6. **Configurable**: Support configuration options for customization
7. **Time-Focused**: All visualizations show **time spent** (work intervals), not task counts

## Command Syntax

### Primary Command: `task <filter> chart [type] [options]`

The `chart` command will be the main entry point for all visualizations, with a `type` subcommand to specify the visualization style.

**Basic Syntax:**
```
task [<filter>] chart [<type>] [<options>]
```

**Examples:**
```
task chart                    # Default: daily timeline
task project:Home chart       # Filtered by project
task chart timeline            # Explicit timeline view
task chart calendar            # Calendar heatmap
task chart project             # Project time allocation
task chart task                # Task time allocation
task chart cumulative          # Cumulative time over period
```

## Visualization Types

### 1. Daily Timeline View (Default)
**Command:** `task [<filter>] chart` or `task [<filter>] chart timeline`

**Description:** Shows intervals as horizontal bars on a timeline, similar to a Gantt chart. Each task/project gets a row, with intervals displayed as colored bars.

**Syntax:**
```
task [<filter>] chart [timeline] [--days=N] [--start=DATE] [--end=DATE] [--granularity=hour|15min|30min]
```

**Options:**
- `--days=N`: Show last N days (default: 7)
- `--start=DATE`: Start date for timeline
- `--end=DATE`: End date for timeline
- `--granularity=hour|15min|30min`: Time resolution (default: hour)

**Output Example:**
```
Timeline View (2025-12-25 to 2025-12-31)
═══════════════════════════════════════════════════════════════════════════
Date:     25-Dec  26-Dec  27-Dec  28-Dec  29-Dec  30-Dec  31-Dec
Time:     08 12 16 20  08 12 16 20  08 12 16 20  08 12 16 20  08 12 16 20  08 12 16 20  08 12 16 20
───────────────────────────────────────────────────────────────────────────
Project A ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
          ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Task 1    ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Task 2    ░░░░░░░░████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Task 3    ░░░░░░░░░░░░░░░░████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
───────────────────────────────────────────────────────────────────────────
Project B ░░░░░░░░░░░░░░░░░░░░░░░░████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Task 4    ░░░░░░░░░░░░░░░░░░░░░░░░████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
───────────────────────────────────────────────────────────────────────────
Legend: █ = Active work  ░ = No work
Total: 12h 34m
```

**Color Coding:**
- Different projects/tasks get different colors
- Active intervals: solid blocks (█)
- Gaps: light blocks (░)
- Current time: vertical line (│) or highlighted column

### 2. Calendar Heatmap View
**Command:** `task [<filter>] chart calendar`

**Description:** Shows a calendar grid with heatmap coloring based on work duration per day. Similar to GitHub contribution graphs. **Note**: This differs from the existing `task calendar` command, which shows due dates, not work intervals.

**Syntax:**
```
task [<filter>] chart calendar [--year=YYYY] [--month=MM] [--weeks=N]
```

**Options:**
- `--year=YYYY`: Show specific year (default: current)
- `--month=MM`: Show specific month (default: current)
- `--weeks=N`: Show last N weeks (default: 12)

**Output Example:**
```
Calendar Heatmap (Last 12 Weeks)
═══════════════════════════════════════════════════════════════════════════
        Mon    Tue    Wed    Thu    Fri    Sat    Sun
Week 1  ░░░    ░░░    ░░░    ░░░    ░░░    ░░░    ░░░
Week 2  ░░░    ███    ███    ███    ███    ░░░    ░░░
Week 3  ███    ███    ███    ███    ███    ░░░    ░░░
Week 4  ███    ███    ███    ███    ███    ░░░    ░░░
Week 5  ███    ███    ███    ███    ███    ░░░    ░░░
Week 6  ███    ███    ███    ███    ███    ░░░    ░░░
Week 7  ███    ███    ███    ███    ███    ░░░    ░░░
Week 8  ███    ███    ███    ███    ███    ░░░    ░░░
Week 9  ███    ███    ███    ███    ███    ░░░    ░░░
Week 10 ███    ███    ███    ███    ███    ░░░    ░░░
Week 11 ███    ███    ███    ███    ███    ░░░    ░░░
Week 12 ███    ███    ███    ███    ███    ░░░    ░░░

Legend: ░░░ = 0h  ░█░ = 1-2h  █░█ = 3-4h  ███ = 5h+
```

**Color Coding:**
- Intensity based on duration: light (0-1h), medium (1-4h), dark (4-8h), very dark (8h+)
- Different colors for different projects (if `--by-project` option used)

### 3. Project Time Allocation
**Command:** `task [<filter>] chart project`

**Description:** Shows time allocation by project using bar charts and/or pie chart representation.

**Syntax:**
```
task [<filter>] chart project [--period=week|month|year|all] [--format=bar|pie|table]
```

**Options:**
- `--period=week|month|year|all`: Time period (default: month)
- `--format=bar|pie|table`: Display format (default: bar)

**Output Example (Bar Chart):**
```
Project Time Allocation (Last Month)
═══════════════════════════════════════════════════════════════════════════
Project A    ████████████████████████████████████████████████████  45h 30m  (45%)
Project B    ████████████████████████████████████████            32h 15m  (32%)
Project C    ████████████████████                                18h 45m  (18%)
Uncategorized ██████                                            4h 30m   (5%)
───────────────────────────────────────────────────────────────────────────
Total: 100h 00m
```

**Output Example (Pie Chart - ASCII):**
```
Project Time Allocation (Last Month)
═══════════════════════════════════════════════════════════════════════════
        ╭─────────────────╮
        │                 │
        │   Project A     │
        │   (45%)         │
        │                 │
        │  ╭───────────╮  │
        │  │ Project B │  │
        │  │  (32%)    │  │
        │  │           │  │
        │  │ ╭───────╮ │  │
        │  │ │Proj C │ │  │
        │  │ │ (18%) │ │  │
        │  │ │       │ │  │
        │  │ │ ╭───╮ │ │  │
        │  │ │ │Unc│ │ │  │
        │  │ │ │(5%)│ │ │  │
        │  │ │ ╰───╯ │ │  │
        │  │ ╰───────╯ │  │
        │  ╰───────────╯  │
        ╰─────────────────╯

Project A: 45h 30m (45%)
Project B: 32h 15m (32%)
Project C: 18h 45m (18%)
Uncategorized: 4h 30m (5%)
Total: 100h 00m
```

### 4. Task Time Allocation
**Command:** `task [<filter>] chart task`

**Description:** Shows time allocation by individual task, useful for identifying time sinks or understanding task-level effort.

**Syntax:**
```
task [<filter>] chart task [--limit=N] [--period=week|month|year|all] [--min-duration=DURATION]
```

**Options:**
- `--limit=N`: Show top N tasks (default: 20)
- `--period=week|month|year|all`: Time period (default: month)
- `--min-duration=DURATION`: Minimum duration to include (e.g., `1h`, `30min`)

**Output Example:**
```
Top Tasks by Time (Last Month)
═══════════════════════════════════════════════════════════════════════════
#1  Task: Implement feature X        ████████████████████████████  12h 30m
    Project: Project A
    
#2  Task: Fix critical bug           ████████████████████████      10h 15m
    Project: Project B
    
#3  Task: Code review                ████████████████████          8h 45m
    Project: Project A
    
#4  Task: Write documentation        ████████████████              6h 30m
    Project: Project C
    
#5  Task: Refactor module            ██████████████                5h 15m
    Project: Project A
    
───────────────────────────────────────────────────────────────────────────
Total: 43h 15m (showing top 5 of 20)
```

### 5. Daily/Weekly Breakdown
**Command:** `task [<filter>] chart daily` or `task [<filter>] chart weekly`

**Description:** Shows work hours by day of week or by date, useful for identifying patterns (e.g., "I work more on Tuesdays").

**Syntax:**
```
task [<filter>] chart daily [--weeks=N] [--group-by=day|date]
task [<filter>] chart weekly [--weeks=N]
```

**Options:**
- `--weeks=N`: Number of weeks to analyze (default: 4)
- `--group-by=day|date`: Group by day of week or specific dates (default: day)

**Output Example (Daily - by Day of Week):**
```
Daily Work Pattern (Last 4 Weeks)
═══════════════════════════════════════════════════════════════════════════
Monday      ████████████████████████████████████████████████████  8h 15m
Tuesday     ████████████████████████████████████████████████████  8h 30m
Wednesday   ████████████████████████████████████████████████████  8h 00m
Thursday    ████████████████████████████████████████████████████  7h 45m
Friday      ████████████████████████████████████████████████████  6h 30m
Saturday    ████████                                                1h 00m
Sunday      ████████                                                1h 00m
───────────────────────────────────────────────────────────────────────────
Average per day: 5h 57m
Total: 40h 00m
```

**Output Example (Daily - by Date):**
```
Daily Work Hours (Last 4 Weeks)
═══════════════════════════════════════════════════════════════════════════
2025-12-01  ████████████████████████████████████████████████████  8h 15m
2025-12-02  ████████████████████████████████████████████████████  8h 30m
2025-12-03  ████████████████████████████████████████████████████  8h 00m
2025-12-04  ████████████████████████████████████████████████████  7h 45m
2025-12-05  ████████████████████████████████████████████████████  6h 30m
2025-12-06  ████████                                                1h 00m
2025-12-07  ████████                                                1h 00m
...
───────────────────────────────────────────────────────────────────────────
Total: 40h 00m
```

### 6. Cumulative Time View
**Command:** `task [<filter>] chart cumulative`

**Description:** Shows cumulative work time over a period, useful for tracking progress toward goals or understanding trends. **Note**: This differs from burndown charts, which show task counts, not cumulative time spent.

**Syntax:**
```
task [<filter>] chart cumulative [--period=week|month|year] [--by=project|task|day]
```

**Options:**
- `--period=week|month|year`: Time period (default: month)
- `--by=project|task|day`: Grouping (default: day)

**Output Example:**
```
Cumulative Work Time (Last Month)
═══════════════════════════════════════════════════════════════════════════
Date       Daily    Cumulative    Trend
───────────────────────────────────────────────────────────────────────────
2025-12-01  8h 15m    8h 15m      ╱
2025-12-02  8h 30m   16h 45m      ╱
2025-12-03  8h 00m   24h 45m      ╱
2025-12-04  7h 45m   32h 30m      ╱
2025-12-05  6h 30m   39h 00m      ╱
2025-12-06  1h 00m   40h 00m      ╱
2025-12-07  1h 00m   41h 00m      ╱
...
───────────────────────────────────────────────────────────────────────────
Total: 160h 00m
Average per day: 5h 10m
```

### 7. Interval Gantt View
**Command:** `task [<filter>] chart gantt`

**Description:** Shows intervals as a traditional Gantt chart with tasks/projects as rows and time on the x-axis.

**Syntax:**
```
task [<filter>] chart gantt [--start=DATE] [--end=DATE] [--group-by=project|task]
```

**Options:**
- `--start=DATE`: Start date (default: 7 days ago)
- `--end=DATE`: End date (default: today)
- `--group-by=project|task`: Grouping level (default: task)

**Output Example:**
```
Gantt Chart View (2025-12-25 to 2025-12-31)
═══════════════════════════════════════════════════════════════════════════
            Dec 25    Dec 26    Dec 27    Dec 28    Dec 29    Dec 30    Dec 31
            ──────    ──────    ──────    ──────    ──────    ──────    ──────
Project A
  Task 1    [████████]                                    [████]
  Task 2              [████████████]
  Task 3                                    [████████████████████]
Project B
  Task 4    [████]    [████]    [████]    [████]    [████]
───────────────────────────────────────────────────────────────────────────
Legend: [████] = Work interval
```

## Implementation Details

### Command Structure

**New Command:** `CmdChart` in `src/commands/CmdChart.h` and `CmdChart.cpp`

**Command Properties:**
- `_keyword = "chart"`
- `_usage = "task [<filter>] chart [<type>] [<options>]"`
- `_description = "Shows graphical visualizations of work intervals"`
- `_read_only = true`
- `_accepts_filter = true`
- `_accepts_modifications = false`
- `_category = Command::Category::metadata`

### Visualization Engine

**New Component:** `src/ChartRenderer.h` and `ChartRenderer.cpp`

This will provide:
- Unicode box-drawing character support
- Color management (integrating with Taskwarrior's Color class)
- Bar chart rendering
- Timeline rendering
- Calendar grid rendering
- Text-based chart rendering utilities

**Key Functions:**
```cpp
class ChartRenderer {
  // Render a horizontal bar chart
  std::string renderBarChart(const std::vector<BarData>& data, int width);
  
  // Render a timeline view
  std::string renderTimeline(const TimelineData& data, const ChartOptions& opts);
  
  // Render a calendar heatmap
  std::string renderCalendar(const CalendarData& data, const ChartOptions& opts);
  
  // Render a cumulative line chart (ASCII)
  std::string renderCumulative(const CumulativeData& data, const ChartOptions& opts);
  
  // Get color for project/task (consistent color assignment)
  Color getColorForItem(const std::string& identifier);
};
```

### Data Aggregation

**New Component:** `src/IntervalAggregator.h` and `IntervalAggregator.cpp`

This will provide:
- Aggregation by project
- Aggregation by task
- Aggregation by date/day of week
- Time range filtering
- Cumulative calculations

**Key Functions:**
```cpp
class IntervalAggregator {
  // Get intervals grouped by project
  std::map<std::string, time_t> aggregateByProject(
    const std::vector<Task>& tasks,
    const DateRange& range
  );
  
  // Get intervals grouped by task
  std::map<int, time_t> aggregateByTask(
    const std::vector<Task>& tasks,
    const DateRange& range
  );
  
  // Get intervals grouped by day of week
  std::map<int, time_t> aggregateByDayOfWeek(
    const std::vector<Task>& tasks,
    const DateRange& range
  );
  
  // Get intervals for timeline view
  TimelineData getTimelineData(
    const std::vector<Task>& tasks,
    const DateRange& range,
    const std::string& granularity
  );
};
```

### Color Scheme

**Color Assignment Strategy:**
1. Projects get consistent colors based on hash of project name
2. Tasks within a project get variations of the project color
3. Use Taskwarrior's existing color palette
4. Support monochrome mode (graceful degradation)

**Configuration Options:**
- `chart.colors.enabled` (default: based on `color` setting)
- `chart.colors.project.<project_name>` (custom project colors)
- `chart.width` (default: terminal width)

### Unicode Character Support

**Characters to Use:**
- Blocks: `█` (full), `▓` (75%), `▒` (50%), `░` (25%), ` ` (empty)
- Box drawing: `─`, `│`, `┌`, `┐`, `└`, `┘`, `├`, `┤`, `┬`, `┴`, `┼`
- Braille: `⠁`, `⠂`, `⠃`, `⠄`, `⠅`, `⠆`, `⠇`, `⠈`, `⠉`, `⠊`, `⠋`, `⠌`, `⠍`, `⠎`, `⠏` (for density)
- Arrows: `╱`, `╲`, `→`, `←`, `↑`, `↓`

**Fallback for Non-Unicode Terminals:**
- Use ASCII: `#`, `=`, `-`, `|`, `+`, `.`, `:`

## Configuration

### New Configuration Variables

```ini
# Chart command settings
chart.default.type=timeline
chart.default.days=7
chart.width=auto              # auto, or number
chart.colors.enabled=on       # on, off, auto
chart.colors.project.<name>=<color>
chart.granularity=hour        # hour, 15min, 30min
```

## Examples

### Basic Usage
```bash
# Show default timeline for last 7 days
task chart

# Show calendar heatmap for last 12 weeks
task chart calendar

# Show project time allocation for last month
task chart project --period=month

# Show top 10 tasks by time
task chart task --limit=10

# Show daily pattern for last 4 weeks
task chart daily --weeks=4

# Show cumulative time for current month
task chart cumulative --period=month
```

### With Filters
```bash
# Show timeline for specific project
task project:Home chart timeline

# Show calendar for tasks with specific tag
task +urgent chart calendar

# Show project allocation for date range
task start.after:2025-12-01 start.before:2025-12-31 chart project

# Show task allocation for specific project
task project:Work chart task
```

### Combined Views
```bash
# Show both table and chart
task intervals && task chart timeline

# Show multiple chart types
task chart project && task chart daily
```

## Use Cases

### Project Management
1. **Time Allocation Review**: `task chart project` - See where time is being spent
2. **Weekly Planning**: `task chart weekly` - Understand weekly patterns
3. **Project Progress**: `task project:X chart cumulative` - Track progress over time
4. **Resource Planning**: `task chart gantt` - See overlapping work periods

### Self-Management
1. **Productivity Patterns**: `task chart daily --group-by=day` - Identify most productive days
2. **Work-Life Balance**: `task chart calendar` - Visualize work distribution
3. **Time Tracking**: `task chart cumulative` - Track total hours worked
4. **Task Analysis**: `task chart task --limit=20` - Identify time-consuming tasks

### Reporting
1. **Weekly Reports**: `task chart weekly --weeks=1`
2. **Monthly Reports**: `task chart project --period=month`
3. **Yearly Reviews**: `task chart calendar --year=2025`

## Implementation Phases

### Phase 1: Core Infrastructure
- [ ] Create `CmdChart` command structure
- [ ] Create `ChartRenderer` class with basic rendering utilities
- [ ] Create `IntervalAggregator` class for data processing
- [ ] Implement basic bar chart rendering
- [ ] Add color support integration

### Phase 2: Timeline View
- [ ] Implement timeline data aggregation
- [ ] Render timeline with Unicode characters
- [ ] Add granularity options (hour, 15min, 30min)
- [ ] Support date range filtering
- [ ] Add current time indicator

### Phase 3: Calendar Heatmap
- [ ] Implement calendar grid rendering
- [ ] Add heatmap color intensity
- [ ] Support week/month/year views
- [ ] Add legend and statistics

### Phase 4: Project/Task Allocation
- [ ] Implement project aggregation
- [ ] Implement task aggregation
- [ ] Add bar chart rendering
- [ ] Add pie chart rendering (ASCII)
- [ ] Add percentage calculations

### Phase 5: Daily/Weekly Breakdown
- [ ] Implement day-of-week aggregation
- [ ] Implement date-based aggregation
- [ ] Add bar chart rendering for daily patterns
- [ ] Add statistics (average, total)

### Phase 6: Cumulative View
- [ ] Implement cumulative calculations
- [ ] Add line chart rendering (ASCII)
- [ ] Support different grouping options
- [ ] Add trend indicators

### Phase 7: Gantt View
- [ ] Implement Gantt chart data structure
- [ ] Render intervals as horizontal bars
- [ ] Support project/task grouping
- [ ] Add date axis labels

### Phase 8: Polish
- [ ] Add configuration options
- [ ] Improve Unicode fallback
- [ ] Add help text
- [ ] Write tests
- [ ] Update documentation

## Testing Strategy

1. **Unit Tests**: Test aggregation functions, rendering utilities
2. **Integration Tests**: Test command parsing, filter application
3. **Visual Tests**: Verify output looks correct in different terminals
4. **Color Tests**: Test color rendering in different color modes
5. **Unicode Tests**: Test fallback behavior for non-Unicode terminals

## Documentation

- Update `task.1` man page with `chart` command documentation
- Add examples to `task-color.5` for chart color customization
- Create tutorial in documentation
- Add to `task help` output

## Future Enhancements

1. **Export Options**: Export charts as text files, or integrate with external tools
2. **Interactive Mode**: Allow zooming/panning in timeline views
3. **Comparison Views**: Compare time allocation across different periods
4. **Goal Tracking**: Set time goals and show progress
5. **Integration**: Export data for external visualization tools (gnuplot, etc.)
6. **Combined Views**: Option to show both task counts (burndown-style) and time spent side-by-side
7. **Burndown Integration**: Add time-based burndown showing hours remaining vs. hours worked

## Avoiding Duplication Summary

**What we're NOT duplicating:**
- Task count visualizations (burndown charts already do this)
- Task history counts (ghistory already does this)
- Due date calendars (calendar command already does this)
- Task event listings (timesheet already does this)

**What we're adding (unique value):**
- **Time-based visualizations**: All our charts show **time spent** (work intervals), not task counts
- **Time allocation**: Project/task time distribution
- **Time patterns**: Daily/weekly time patterns
- **Interval visualization**: Gantt/timeline views of actual work intervals
- **Cumulative time tracking**: Running totals of time spent

This ensures our new visualizations complement existing features rather than duplicate them.
