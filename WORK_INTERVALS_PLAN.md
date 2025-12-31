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
    message TEXT,                      -- Optional commit message (NULL for start events)
    interval_id INTEGER,                -- Links start/stop/done events that form an interval
    created_at INTEGER NOT NULL,       -- When this record was created
    FOREIGN KEY (task_uuid) REFERENCES tasks(uuid) ON DELETE CASCADE
);

CREATE INDEX idx_work_intervals_task_uuid ON work_intervals(task_uuid);
CREATE INDEX idx_work_intervals_timestamp ON work_intervals(timestamp);
CREATE INDEX idx_work_intervals_interval_id ON work_intervals(interval_id);
```

**Design Notes:**
- `interval_id` groups related start/stop/done events into work intervals
- Messages are only stored for 'stop' and 'done' events (NULL for 'start')
- Timestamps use Unix epoch (seconds since 1970-01-01)
- Foreign key ensures data integrity (though TaskChampion may not expose this)

### Component Structure

```
src/
├── WorkInterval.h          # Header for WorkInterval class
├── WorkInterval.cpp        # Implementation of work interval management
├── commands/
│   ├── CmdStart.cpp        # Modified to log start events
│   ├── CmdStop.cpp         # Modified to log stop events + message
│   └── CmdDone.cpp         # Modified to log done events + message
└── TDB2.h/cpp              # May need to expose database path
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
                         const std::string& event_type,
                         time_t timestamp,
                         const std::string& message = "");
    static std::vector<Interval> get_intervals(const std::string& task_uuid);
    static std::vector<Interval> get_intervals_by_date(time_t start, time_t end);
};
```

#### Step 1.2: Database Access
**Challenge:** TaskChampion manages the SQLite database. We need direct access.

**Options:**
1. **Direct SQLite access** (Recommended): Open the same database file directly
   - Get database path from `data.location` config
   - Open `{data.location}/taskchampion.sqlite3` directly
   - Use SQLite C API or a lightweight wrapper

2. **Extend TaskChampion**: Add Rust bindings (more complex, requires TaskChampion changes)

**Implementation:**
- Add SQLite3 dependency to CMakeLists.txt (if not already present)
- Use libsqlite3 or rusqlite via FFI
- Initialize table on first use (lazy initialization)

### Phase 2: Command-Line Interface

#### Step 2.1: Extend CLI2 Parser
**File:** `src/CLI2.cpp` and `src/CLI2.h`

Add support for `--message` or `-m` flag:
```bash
task 1 stop --message "Fixed bug in authentication"
task 2 done -m "Completed feature implementation"
```

**Implementation:**
- Add message extraction in `CLI2::getMiscellaneous()` or new method
- Store message in command context
- Make available to command execution

#### Step 2.2: Modify Commands

**CmdStart.cpp:**
```cpp
// After task.setAsNow("start")
WorkInterval::log_event(task.get("uuid"), "start", time(nullptr));
```

**CmdStop.cpp:**
```cpp
// Extract message from CLI2
std::string message = Context::getContext().get_message(); // New method

// Before task.remove("start")
time_t stop_time = time(nullptr);
time_t start_time = task.get_date("start");

// Log stop event
WorkInterval::log_event(task.get("uuid"), "stop", stop_time, message);

// Then proceed with existing stop logic
```

**CmdDone.cpp:**
```cpp
// Extract message from CLI2
std::string message = Context::getContext().get_message();

// Before setting status to completed
time_t done_time = time(nullptr);
if (task.has("start")) {
    time_t start_time = task.get_date("start");
    // Log done event (which also closes the interval)
    WorkInterval::log_event(task.get("uuid"), "done", done_time, message);
    task.remove("start");
}
```

### Phase 3: Query and Display

#### Step 3.1: New Command or Extend Existing
**Option A:** New command `task intervals <filter>`
**Option B:** Extend `task timesheet` or `task history`

**Recommended:** New command `CmdIntervals` for clarity

**File:** `src/commands/CmdIntervals.cpp`

**Features:**
- List intervals for filtered tasks
- Show duration, timestamps, messages
- Filter by date range
- Export to various formats

### Phase 4: Integration Points

#### Step 4.1: Context Integration
**File:** `src/Context.h` and `src/Context.cpp`

Add:
- Message storage from CLI2
- WorkInterval initialization on startup
- Database path accessor

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

- Store as TEXT (SQLite handles this well)
- No length limit (or reasonable limit like 10KB)
- UTF-8 encoding
- Allow multi-line messages (store as-is, display with formatting)

### Database Migration

- Check if table exists on first access
- Create if missing (lazy initialization)
- No migration needed for existing databases (new feature)

## Example Usage

```bash
# Start working on task
task 1 start

# Stop with message
task 1 stop --message "Fixed authentication bug, need to test"

# Resume work
task 1 start

# Complete with message
task 1 done -m "All tests passing, ready for review"

# View intervals
task 1 intervals
# Output:
# Interval 1: 2024-01-15 10:00 - 2024-01-15 11:30 (1h 30m)
#   Message: Fixed authentication bug, need to test
# Interval 2: 2024-01-15 14:00 - 2024-01-15 16:45 (2h 45m)
#   Message: All tests passing, ready for review
```

## Dependencies

- SQLite3 library (likely already available via TaskChampion)
- No new external dependencies if using direct SQLite access

## Future Enhancements

1. **Interval editing**: Modify or delete intervals
2. **Time reports**: Aggregate time by project, tag, date
3. **Export**: Export intervals to CSV, JSON, timewarrior format
4. **Sync**: Include intervals in sync (if TaskChampion supports custom tables)
5. **UI**: Better formatting for interval display

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Database conflicts with TaskChampion | Use separate table, read-only access to TaskChampion tables |
| Performance impact | Index on task_uuid and timestamp, lazy initialization |
| Data loss | SQLite transactions, foreign key constraints |
| Migration complexity | Lazy table creation, no migration needed |

## Success Criteria

1. ✅ Start/stop/done commands log intervals
2. ✅ Messages can be added to stop/done commands
3. ✅ Intervals are queryable by task UUID
4. ✅ Intervals persist across sessions
5. ✅ Interval durations are calculable
6. ✅ Tests pass

## Timeline Estimate

- Phase 1 (Infrastructure): 2-3 days
- Phase 2 (CLI Integration): 1-2 days
- Phase 3 (Query/Display): 1-2 days
- Phase 4 (Integration): 1 day
- Phase 5 (Testing): 2-3 days

**Total: 7-11 days** for a complete implementation
