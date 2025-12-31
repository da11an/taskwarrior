# Work Intervals Tracking Implementation Plan

## Overview
Add time tracking functionality to Taskwarrior by logging work intervals (start/stop/done events) with optional commit-message-like notes. This creates a timewarrior-like feature integrated directly with tasks.

## Goals
1. Track work intervals (start/stop/done) with timestamps
2. Allow optional messages on stop and done commands
3. Store intervals in SQLite table separate from task data
4. Map intervals to task UUIDs (permanent identifiers)

## Architecture

### Database Schema

Create a new table `work_intervals` in the same SQLite database (`taskchampion.sqlite3`):

```sql
CREATE TABLE IF NOT EXISTS work_intervals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_uuid TEXT NOT NULL,           -- Task UUID (permanent identifier)
    event_type TEXT NOT NULL,          -- 'start', 'stop', or 'done'
    timestamp INTEGER NOT NULL,        -- Unix timestamp
    interval_id INTEGER,                -- Links start/stop/done events that form an interval
    created_at INTEGER NOT NULL        -- When this record was created
);

CREATE INDEX idx_work_intervals_task_uuid ON work_intervals(task_uuid);
CREATE INDEX idx_work_intervals_timestamp ON work_intervals(timestamp);
CREATE INDEX idx_work_intervals_interval_id ON work_intervals(interval_id);
```

**Design Notes:**
- `interval_id` groups related start/stop/done events into work intervals
- **Messages are NOT stored in this table** - they are stored as Taskwarrior annotations with timestamps matching the interval events
- Timestamps use Unix epoch (seconds since 1970-01-01)
- This design leverages Taskwarrior's existing annotation system, making messages visible in `task info` and avoiding data duplication

### Component Structure

```
src/
├── WorkInterval.h          # Header for WorkInterval class
├── WorkInterval.cpp        # Implementation of work interval management
├── commands/
│   ├── CmdStart.cpp        # Modified to log start events (no messages)
│   ├── CmdStop.cpp         # Modified to log stop events + add message as annotation
│   ├── CmdDone.cpp         # Modified to log done events + add message as annotation
│   ├── CmdIntervals.cpp    # New command to display intervals with annotations
│   └── CmdSummary.cpp      # Modified to show total duration per project
├── columns/
│   ├── ColWorkDuration.h   # New column for work duration in list view
│   └── ColWorkDuration.cpp
├── CLI2.h/cpp              # Modified to parse --message/-m flags
├── Context.h/cpp            # Modified to store message and initialize WorkInterval
└── Task.h/cpp               # Modified to add annotation overload with timestamp
```

## Implementation Steps

### Phase 1: Core Infrastructure

#### Step 1.1: Create WorkInterval Class
**File:** `src/WorkInterval.h` and `src/WorkInterval.cpp`

**Responsibilities:**
- Database initialization (create table if not exists)
- Insert interval events
- Query intervals by task UUID
- Query intervals by date range
- Calculate interval durations

**Key Methods:**
```cpp
class WorkInterval {
public:
    static void initialize(const std::string& db_path);
    static void log_event(const std::string& task_uuid, 
                         const std::string& event_type,  // "start", "stop", or "done"
                         time_t timestamp);
    static std::vector<Interval> get_intervals(const std::string& task_uuid);
    // Note: Messages are handled separately via Task annotations
};
```

**Design Decision:** Messages are not passed to `log_event` because they are stored as Taskwarrior annotations with timestamps matching the interval events. This avoids duplication and makes messages visible in `task info`.

#### Step 1.2: Database Access
**Challenge:** TaskChampion manages the SQLite database. We need direct access.

**Options:**
1. **Direct SQLite access** (Recommended): Open the same database file directly
   - Get database path from `data.location` config
   - Open `{data.location}/taskchampion.sqlite3` directly
   - Use SQLite C API or a lightweight wrapper

2. **Extend TaskChampion**: Add Rust bindings (more complex, requires TaskChampion changes)

**Implementation:**
- ✅ Added SQLite3 dependency to CMakeLists.txt (`target_link_libraries(task ... sqlite3)`)
- ✅ Using libsqlite3 directly via SQLite C API
- ✅ Initialize table on first use via `ensure_table_exists()` method
- ✅ Exception handling: throws `std::string` (not `std::runtime_error`) to match Taskwarrior's error handling pattern

### Phase 2: Command-Line Interface

#### Step 2.1: Extend CLI2 Parser
**File:** `src/CLI2.cpp` and `src/CLI2.h`

Add support for `--message` or `-m` flag:
```bash
task 1 stop --message "Fixed bug in authentication"
task 2 done -m "Completed feature implementation"
```

**Implementation:**
- ✅ Added `getMessage()` method to `CLI2` class
- ✅ Added `_message` member to store extracted message
- ✅ Updated `categorizeArgs()` to handle commands that accept filter, modifications, and miscellaneous arguments
- ✅ Message values are tagged as `MISCELLANEOUS` (not `MODIFICATION`) to prevent them from being added as annotations automatically
- ✅ Message stored in `Context` via `_message` member and `get_message()` accessor

#### Step 2.2: Modify Commands

**CmdStart.cpp:**
```cpp
// After task.setAsNow("start")
WorkInterval::log_event(task.get("uuid"), "start", time(nullptr));
// Note: Start command does NOT accept messages
```

**CmdStop.cpp:**
```cpp
// Set _accepts_miscellaneous = true in constructor
// Extract message from CLI2
std::string message = Context::getContext().cli2.getMessage();

// Before task.remove("start")
time_t stop_time = time(nullptr);

// Log stop event (no message parameter)
WorkInterval::log_event(task.get("uuid"), "stop", stop_time);

// Add message as annotation with stop timestamp if provided
if (!message.empty()) {
    task.addAnnotation(message, stop_time);
}

// Then proceed with existing stop logic
```

**CmdDone.cpp:**
```cpp
// Set _accepts_miscellaneous = true in constructor
// Extract message from CLI2
std::string message = Context::getContext().cli2.getMessage();

// Before setting status to completed
time_t done_time = time(nullptr);
if (task.has("start")) {
    // Log done event (no message parameter)
    WorkInterval::log_event(task.get("uuid"), "done", done_time);
    
    // Add message as annotation with done timestamp if provided
    if (!message.empty()) {
        task.addAnnotation(message, done_time);
    }
    
    task.remove("start");
}
```

**Key Design Decisions:**
- Messages are stored as Taskwarrior annotations (not in `work_intervals` table)
- Annotations use timestamps matching the interval event times
- Added `Task::addAnnotation(const std::string&, time_t)` overload to support timestamped annotations
- `Task::modify()` was updated to skip `MISCELLANEOUS` arguments to prevent message values from being added as annotations twice

### Phase 3: Query and Display

#### Step 3.1: New Command `CmdIntervals`
**File:** `src/commands/CmdIntervals.cpp` and `src/commands/CmdIntervals.h`

**Features:**
- ✅ List intervals for filtered tasks
- ✅ Show duration, timestamps (start and end)
- ✅ Display annotations categorized into three columns:
  - **Start Note**: Annotations within 2 seconds of interval start
  - **During**: Annotations between start and end (excluding edge cases)
  - **End Note**: Annotations within 2 seconds of interval end
- ✅ Multiple annotations are joined with semicolons
- ✅ Shows total duration at the bottom if multiple intervals exist
- ✅ Uses `Table` class for formatted output with proper column alignment

**Example Output:**
```
ID Start              End                Duration Start Note During End Note
-- ------------------ ------------------ -------- ---------- ------ --------
 1 2024-01-15 10:00:00 2024-01-15 11:30:00 1:30:00           Fixed bug
 1 2024-01-15 14:00:00 2024-01-15 16:45:00 2:45:00           All tests passing

Total time: 4:15:00
```

#### Step 3.2: New Column `workduration`
**File:** `src/columns/ColWorkDuration.cpp` and `src/columns/ColWorkDuration.h`

**Features:**
- ✅ Displays total duration of all work intervals for a task
- ✅ Added to default `list` report configuration
- ✅ Label: "Duration" (user preference over "Work")
- ✅ Right-aligned, formatted using `Duration::format()`

#### Step 3.3: Summary Command Enhancement
**File:** `src/commands/CmdSummary.cpp`

**Features:**
- ✅ Added "Duration" column showing total duration per project
- ✅ Sums all work intervals for all tasks in each project
- ✅ Includes parent projects in hierarchical project structures

### Phase 4: Integration Points

#### Step 4.1: Context Integration
**File:** `src/Context.h` and `src/Context.cpp`

**Implemented:**
- ✅ Added `_message` member to `Context` class
- ✅ Added `get_message()` accessor method
- ✅ `WorkInterval::initialize()` called after TaskChampion replica is opened
- ✅ Database path obtained from `data.location` config + `/taskchampion.sqlite3`
- ✅ Default `report.list.columns` and `report.list.labels` updated to include `workduration`

#### Step 4.2: TDB2 Integration
**File:** `src/TDB2.h` and `src/TDB2.cpp`

May need to expose:
- Database path (if not already accessible)
- Or add WorkInterval initialization call

### Phase 5: Testing

#### Step 5.1: Unit Tests
**File:** `test/work_intervals.test.py`

Test cases:
- Interval creation (start/stop/done)
- Message storage
- Interval querying
- Multiple intervals per task
- Interval duration calculation

#### Step 5.2: Integration Tests
- Full workflow: start -> stop (with message) -> done (with message)
- Verify database persistence
- Verify interval linking

## Technical Considerations

### SQLite Access Strategy

Since TaskChampion manages the database, we have two approaches:

**Approach 1: Direct SQLite Access (Recommended)**
- Pros: Simple, no TaskChampion changes needed
- Cons: Bypasses TaskChampion abstraction, potential conflicts
- Implementation: Use SQLite C API or rusqlite via FFI

**Approach 2: Extend TaskChampion**
- Pros: Cleaner architecture, uses existing infrastructure
- Cons: Requires TaskChampion changes, more complex
- Implementation: Add Rust functions to TaskChampion, expose via C++ bindings

**Recommendation:** Start with Approach 1 for MVP, consider Approach 2 for long-term.

### Interval ID Generation

Options:
1. Auto-increment per task (simple)
2. UUID-based (more robust)
3. Timestamp-based (human-readable)

**Recommendation:** Use auto-increment per task, reset on each start event.

### Message Storage

**Design Decision:** Messages are stored as Taskwarrior annotations, not in the `work_intervals` table.

**Rationale:**
- Leverages existing Taskwarrior annotation system
- Messages visible in `task info` command
- Avoids data duplication
- Annotations are timestamped, matching interval event times
- No need for separate message storage mechanism

**Implementation:**
- Added `Task::addAnnotation(const std::string&, time_t)` overload
- Messages added with timestamps matching stop/done event times
- Annotations stored in task data as `annotation_<timestamp>` keys
- UTF-8 encoding, no length limit (Taskwarrior handles this)
- Multi-line messages supported (stored as-is)

### Database Migration

- Check if table exists on first access
- Create if missing (lazy initialization)
- No migration needed for existing databases (new feature)

## Example Usage

```bash
# Start working on task
task 1 start

# Stop with message (message becomes an annotation)
task 1 stop --message "Fixed authentication bug, need to test"

# Resume work
task 1 start

# Complete with message (message becomes an annotation)
task 1 done -m "All tests passing, ready for review"

# View intervals with annotations
task 1 intervals
# Output:
# ID Start              End                Duration Start Note During End Note
# -- ------------------ ------------------ -------- ---------- ------ --------
#  1 2024-01-15 10:00:00 2024-01-15 11:30:00 1:30:00           Fixed bug
#  1 2024-01-15 14:00:00 2024-01-15 16:45:00 2:45:00           All tests passing
#
# Total time: 4:15:00

# View task info (annotations visible here too)
task 1 info

# List tasks with duration column
task list
# Shows Duration column with total time per task

# Summary by project with total duration
task summary
# Shows Duration column with total time per project
```

## Dependencies

- SQLite3 library (likely already available via TaskChampion)
- No new external dependencies if using direct SQLite access

## Future Enhancements

1. **Interval editing**: Modify or delete intervals
2. **Time reports**: Aggregate time by project, tag, date (partially implemented in summary)
3. **Export**: Export intervals to CSV, JSON, timewarrior format
4. **Sync**: Include intervals in sync (if TaskChampion supports custom tables)
5. **UI**: Better formatting for interval display (currently using Table class)
6. **Date range filtering**: Filter intervals by date range in `intervals` command
7. **Duration formatting options**: Configurable duration display formats

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Database conflicts with TaskChampion | Use separate table, read-only access to TaskChampion tables |
| Performance impact | Index on task_uuid and timestamp, lazy initialization |
| Data loss | SQLite transactions, foreign key constraints |
| Migration complexity | Lazy table creation, no migration needed |

## Success Criteria

1. ✅ Start/stop/done commands log intervals
2. ✅ Messages can be added to stop/done commands (stored as annotations)
3. ✅ Intervals are queryable by task UUID
4. ✅ Intervals persist across sessions
5. ✅ Interval durations are calculable
6. ✅ New `intervals` command displays intervals with categorized annotations
7. ✅ New `workduration` column shows total duration in list view
8. ✅ Summary command shows total duration per project
9. ⏳ Tests pass (pending - Phase 5 not yet completed)

## Timeline Estimate

- Phase 1 (Infrastructure): 2-3 days
- Phase 2 (CLI Integration): 1-2 days
- Phase 3 (Query/Display): 1-2 days
- Phase 4 (Integration): 1 day
- Phase 5 (Testing): 2-3 days

**Total: 7-11 days** for a complete implementation

## Implementation Status

### Completed (Phases 1-4)

✅ **Phase 1: Core Infrastructure**
- WorkInterval class created with SQLite direct access
- Database table creation with proper indexes
- Event logging (start/stop/done) without messages
- Interval querying by task UUID
- Exception handling using `std::string` (matching Taskwarrior pattern)

✅ **Phase 2: Command-Line Interface**
- CLI2 extended to parse `--message`/`-m` flags
- Message extraction and storage in Context
- CmdStart modified (no message support)
- CmdStop modified (accepts messages, stores as annotations)
- CmdDone modified (accepts messages, stores as annotations)
- Task::addAnnotation overload added for timestamped annotations

✅ **Phase 3: Query and Display**
- New `intervals` command with table output
- Annotation categorization (Start Note, During, End Note)
- New `workduration` column for list view
- Summary command enhanced with duration aggregation

✅ **Phase 4: Integration Points**
- Context integration (message storage, WorkInterval initialization)
- Default report configuration updated
- Database path resolution from config

### Pending

⏳ **Phase 5: Testing**
- Unit tests for WorkInterval class
- Integration tests for full workflow
- Test message storage as annotations
- Test interval querying and display

## Key Design Decisions Made

1. **Messages as Annotations**: Instead of storing messages in the `work_intervals` table, messages are stored as Taskwarrior annotations with timestamps matching interval events. This avoids duplication and makes messages visible in `task info`.

2. **No Messages on Start**: The `start` command does not accept messages, only `stop` and `done` commands do.

3. **Exception Handling**: Uses `std::string` exceptions (not `std::runtime_error`) to match Taskwarrior's error handling pattern.

4. **Column Label**: Uses "Duration" instead of "Work" based on user preference.

5. **Annotation Categorization**: The `intervals` command categorizes annotations into three columns based on their timestamps relative to interval start/end times, with a 2-second tolerance for edge cases.

6. **Direct SQLite Access**: Uses direct SQLite C API access rather than extending TaskChampion, keeping the implementation simpler and independent.
