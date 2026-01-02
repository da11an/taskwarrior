# Work Interval Editing Implementation Plan

## Overview
Add the ability to edit work intervals after the fact, similar to Timewarrior's interval editing capabilities. This allows users to correct timestamps, adjust intervals, and fill gaps between intervals.

## Goals
1. Display interval IDs in the intervals table
2. Allow modifying intervals using Taskwarrior's standard syntax
3. Support editing start time, stop time, or both (range)
4. Support "filling" intervals to stretch to meet neighboring intervals (task-agnostic)

## User Interface Design

### 1. Display Interval IDs

**Current State:**
- `task intervals` shows: ID (task ID), Start, End, Duration, Start Note, During, End Note
- Interval IDs exist but are not displayed

**Changes:**
- Add "Interval" column showing `interval_id` (e.g., "1", "2", "3")
- Update column order: Task ID, Interval, Start, End, Duration, Start Note, During, End Note
- Format: Display interval ID as a simple integer

**Files to Modify:**
- `src/commands/CmdIntervals.cpp`: Add interval ID column

### 2. Interval Identifier Syntax

**Proposed Syntax:**
```
task <task-id>.<interval-id> modify <attribute> <value>
```

**Examples:**
```
task 5.2 modify start:2024-01-15T10:00:00
task 5.2 modify stop:2024-01-15T12:30:00
task 5.2 modify start:2024-01-15T10:00:00 stop:2024-01-15T12:30:00
task 1.1,2.3,5.2 modify start:-1h
```

**Decision:** Use dot (`.`) syntax to avoid conflicts with ID ranges (which use hyphens like `1-5`).

**Implementation:**
- Extend `Filter` to recognize `task-id.interval-id` pattern
- Create a new filter type or extend existing filter to support interval selection
- When filter matches `X.Y` where Y is a small integer (interval ID), treat as interval selector
- Store interval ID in filter context for later retrieval

**Files to Modify:**
- `src/Filter.cpp`: Add interval ID parsing logic
- `src/CLI2.cpp`: May need updates for parsing
- `src/commands/CmdModify.cpp`: Handle interval modifications

### 3. Interval Attributes

**Attributes (when in interval context):**
- `start:<datetime>` - Modify start time of an interval
- `stop:<datetime>` - Modify stop time of an interval
- `end:<datetime>` - Alias for `stop` (for consistency with task attributes)

**Note:** When using `task <task-id>.<interval-id> modify`, the interval context is already established, so simple attribute names (`start:`, `stop:`, `end:`) are used rather than prefixed names. This is consistent with Taskwarrior's existing pattern where `task 5 modify start:...` modifies a task's start attribute.

**Behavior Without Interval ID (e.g., `task 5 modify`):**
- **Backward Compatibility:** `task 5 modify start:...` continues to modify the task's `start` attribute (when task was started, makes it active). This maintains full backward compatibility.
- **Backward Compatibility:** `task 5 modify end:...` continues to modify the task's `end` attribute (when task was completed/deleted).
- **Error for Ambiguous Case:** `task 5 modify stop:...` produces an error: "The 'stop' attribute is not valid for tasks. Use 'task <id>.<interval-id> modify stop:...' to modify an interval, or use 'task stop' command to stop a task."
- **Rationale:** Tasks don't have a `stop:` attribute (only `start:` and `end:`), so `stop:` is unambiguous - it can only refer to intervals. This makes the behavior predictable and prevents accidental interval modifications.

**Attribute Parsing:**
- When filter contains interval selector (`X.Y`), route `start:`, `stop:`, and `end:` attributes to interval modification instead of task modification
- When filter does NOT contain interval selector, `start:` and `end:` modify task attributes (backward compatible), and `stop:` produces an error
- Extend `Task::modify()` or create new `WorkInterval::modify()` method
- Parse these attributes specially when in interval context (not as task attributes)
- Support relative times: `start:-1h`, `stop:+30min`

**Files to Modify:**
- `src/Task.cpp`: Add interval attribute parsing in `modify()`
- `src/WorkInterval.h` / `src/WorkInterval.cpp`: Add `modify_interval()` method
- `src/taskchampion-cpp/src/lib.rs`: Add `work_interval_update_timestamp()` function

### 4. Interval Modification Logic

**Validation Rules:**
1. Start time must be before stop time
2. Cannot create overlapping intervals for the same task
3. Cannot modify an interval that doesn't exist
4. For open intervals, can only modify start time (stop is "now")
5. When modifying stop time of open interval, it becomes closed

**Update Strategy:**
- Update the timestamp of the corresponding event in `work_intervals` table
- If modifying start: Update the "start" event's timestamp
- If modifying stop: Update the "stop" or "done" event's timestamp
- If modifying both: Update both events
- Recalculate duration automatically

**Edge Cases:**
- What if modifying start time makes it overlap with previous interval?
  - Option A: Reject with error
  - Option B: Automatically adjust previous interval's stop time
  - **Decision:** Option A (reject with clear error message)
  
- What if modifying stop time makes it overlap with next interval?
  - Same options as above
  - **Decision:** Option A (reject with clear error message)

**Files to Modify:**
- `src/WorkInterval.cpp`: Add `modify_interval()` method
- `src/taskchampion-cpp/src/lib.rs`: Add `work_interval_update_timestamp()`

### 5. Fill Command

**Proposed Syntax:**
```
task <task-id>.<interval-id> fill
task <task-id>.<interval-id> fill start
task <task-id>.<interval-id> fill stop
task <task-id>.<interval-id> fill both
```

**Behavior:**
- `fill` (no args) or `fill both`: Stretch interval to meet both previous and next intervals
- `fill start`: Stretch start time to meet previous interval's end time
- `fill stop`: Stretch stop time to meet next interval's start time

**Task-Agnostic Filling:**
- When filling, look for neighboring intervals across ALL tasks (not just the same task)
- Find the closest interval before (for start fill) or after (for stop fill)
- Stretch to meet that interval's boundary

**Algorithm:**
1. Get all intervals sorted by timestamp
2. Find the target interval
3. For start fill: Find the most recent interval that ends before this interval starts
4. For stop fill: Find the earliest interval that starts after this interval ends
5. Update the interval's start/stop to meet the neighbor

**Files to Create/Modify:**
- `src/commands/CmdFill.h` / `src/commands/CmdFill.cpp`: New command for fill operation
- `src/WorkInterval.cpp`: Add `fill_interval()` method
- `src/taskchampion-cpp/src/lib.rs`: Add `work_interval_fill()` function

## Implementation Steps

### Phase 1: Display Interval IDs
1. ✅ Update `CmdIntervals` to display interval ID column
2. ✅ Test display with various interval configurations

### Phase 2: Filter Parsing for Interval Selection
1. Extend `Filter` to parse `task-id.interval-id` syntax
2. Add interval ID storage to filter context
3. Update filter matching to handle interval selectors
4. Test with various filter combinations

### Phase 3: Interval Modification Infrastructure
1. Add `work_interval_update_timestamp()` to Rust bridge
2. Add `WorkInterval::modify_interval()` method
3. Add validation logic for interval modifications
4. Test timestamp updates

### Phase 4: Attribute Parsing
1. Detect interval context from filter (presence of `X.Y` pattern)
2. When in interval context, route `start:`, `stop:`, and `end:` attributes to `WorkInterval::modify_interval()` instead of task modification
3. Support relative time parsing for intervals
4. Test with various attribute formats

### Phase 5: Fill Command
1. Create `CmdFill` command
2. Implement fill logic in `WorkInterval`
3. Add task-agnostic neighbor finding
4. Test fill operations

## Database Schema

**Current Schema:**
```sql
CREATE TABLE work_intervals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_uuid TEXT NOT NULL,
    event_type TEXT NOT NULL,  -- 'start', 'stop', or 'done'
    timestamp INTEGER NOT NULL,
    interval_id INTEGER,
    created_at INTEGER NOT NULL
);
```

**No Schema Changes Required:**
- We update existing `timestamp` values in the table
- `interval_id` already exists for grouping events
- No new columns needed

## Rust Bridge Functions

**New Functions Needed:**

```rust
// Update a specific event's timestamp
fn work_interval_update_timestamp(
    taskdb_dir: String,
    task_uuid: String,
    interval_id: i32,
    event_type: String,  // "start" or "stop"/"done"
    new_timestamp: i64,
) -> Result<(), CppError>

// Get all intervals across all tasks (for fill operation)
fn work_interval_get_all_intervals(
    taskdb_dir: String,
) -> Result<Vec<ffi::WorkInterval>, CppError>

// Fill an interval to meet neighbors
fn work_interval_fill(
    taskdb_dir: String,
    task_uuid: String,
    interval_id: i32,
    fill_type: String,  // "start", "stop", or "both"
) -> Result<(), CppError>
```

## C++ Interface

**New Methods in WorkInterval:**

```cpp
// Modify an interval's start or stop time
static void modify_interval(
    const std::string& task_uuid,
    int interval_id,
    const std::string& attribute,  // "start", "stop", or "end"
    time_t new_timestamp
);

// Fill an interval to meet neighbors
static void fill_interval(
    const std::string& task_uuid,
    int interval_id,
    const std::string& fill_type  // "start", "stop", or "both"
);

// Get all intervals (for fill operation)
static std::vector<Interval> get_all_intervals();
```

## Error Handling

**Error Messages:**
- "Interval <task-id>.<interval-id> does not exist"
- "Cannot modify interval: start time must be before stop time"
- "Cannot modify interval: would create overlap with interval <task-id>.<interval-id>"
- "Cannot modify stop time of open interval without closing it first"
- "No neighboring interval found for fill operation"
- "The 'stop' attribute is not valid for tasks. Use 'task <id>.<interval-id> modify stop:...' to modify an interval, or use 'task stop' command to stop a task."

## Testing Considerations

1. **Unit Tests:**
   - Filter parsing with interval IDs
   - Interval modification validation
   - Fill operation logic
   - Edge cases (overlaps, open intervals, etc.)

2. **Integration Tests:**
   - End-to-end modification workflow
   - Fill operation across multiple tasks
   - Error handling scenarios

3. **User Testing:**
   - Syntax clarity and discoverability
   - Performance with many intervals
   - Consistency with existing Taskwarrior patterns

## Documentation Updates

1. Update `task intervals` command documentation to show interval ID column
2. Add interval modification examples to user guide
3. Document fill command usage
4. Add interval editing to FAQ/troubleshooting

## Open Questions

1. **Syntax Preference:** Hyphen (`5-2`) vs dot (`5.2`) for interval selection?
   - **Decision:** Dot (`5.2`), to avoid conflicts with ID ranges (which use hyphens like `1-5`)

2. **Fill Behavior:** Should fill respect task boundaries or be truly task-agnostic?
   - **Decision:** Task-agnostic (as specified in requirements)

3. **Overlap Handling:** Auto-adjust vs reject?
   - **Decision:** Reject with clear error (safer, more predictable)

4. **Open Interval Modification:** Allow modifying stop time to close it?
   - **Decision:** Yes, modifying stop time closes the interval

5. **Relative Time Support:** Should `start:-1h` be supported?
   - **Decision:** Yes, for consistency with Taskwarrior's date parsing

6. **Attribute Syntax:** Should we use `interval.start:` or just `start:` when in interval context?
   - **Decision:** Use simple `start:`, `stop:`, `end:` syntax when interval context is established by `X.Y` pattern. This is more consistent with Taskwarrior's existing attribute syntax and less verbose.

7. **Behavior Without Interval ID:** What should `task 5 modify start:` or `task 5 modify stop:` do?
   - **Decision:** 
     - `task 5 modify start:` → modifies task's start attribute (backward compatible)
     - `task 5 modify end:` → modifies task's end attribute (backward compatible)
     - `task 5 modify stop:` → error (tasks don't have stop attribute; must use `5.X` syntax for intervals)
   - **Rationale:** Maintains backward compatibility while making interval modifications explicit and unambiguous.

## Future Enhancements (Out of Scope)

- Bulk interval operations (modify multiple intervals at once)
- Interval merging/splitting
- Undo/redo for interval modifications
- Interval templates or presets
- Export intervals to Timewarrior format
