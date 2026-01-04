# Task Switch Command Implementation Plan

## Overview
Add a new `switch` command that stops any currently active tasks and starts the specified task. This is a convenience command that combines `stop` (for all active tasks) and `start` (for the target task) into a single atomic operation.

## Goals
1. Provide a single command to switch between tasks without manually stopping active tasks first
2. Ensure only one task is active at a time (enforce single-active-task workflow)
3. Maintain consistency with existing `start` and `stop` command behavior
4. Support the same filtering and modification options as `start` and `stop`

## Use Cases

### Primary Use Case: Quick Task Switching
**Problem**: When switching between tasks, users must:
1. Identify which tasks are active
2. Stop all active tasks
3. Start the new task

This is tedious and error-prone.

**Solution**: `task switch <id>` automatically:
1. Finds all active tasks
2. Stops them all
3. Starts the specified task

**Example**:
```bash
# User is working on task 5
task 5 start

# User wants to switch to task 10
task 10 switch  # Stops task 5, starts task 10

# Equivalent to:
# task +ACTIVE stop
# task 10 start
```

### Secondary Use Case: Enforcing Single Active Task
**Problem**: Users may accidentally have multiple tasks active simultaneously, making time tracking ambiguous.

**Solution**: `switch` ensures only one task is active at a time by stopping all others before starting the new one.

## Command Syntax

```
task <filter> switch [<mods>]
```

**Examples**:
- `task 10 switch` - Switch to task 10
- `task project:Work switch` - Switch to first task in Work project
- `task +next switch priority:H` - Switch to next task and set priority to High
- `task switch` - Error: no task specified

## Behavior

### Core Logic
1. **Find active tasks**: Query all tasks with `status:pending -WAITING +ACTIVE` filter
2. **Stop active tasks**: For each active task:
   - Remove the `start` attribute
   - Log work interval stop event
   - Add stop annotation (if journal.time is enabled)
   - Save the task
3. **Start target task**: Apply filter to find target task(s)
   - Validate that exactly one task matches (or handle multiple with confirmation)
   - Set `start` attribute to current time
   - Log work interval start event
   - Add start annotation (if journal.time is enabled)
   - Save the task

### Edge Cases

#### No Active Tasks
- If no tasks are active, `switch` behaves exactly like `start`
- No error or warning needed - this is expected behavior

#### Target Task Already Active
- If the target task is already the only active task, do nothing, report "Task X is already active"
  
#### Multiple Active Tasks
- If multiple tasks are active, stop all of them
- This is the primary use case for `switch`

#### Target Task Already Started (but other tasks also active)
- Stop the target task along with others, then start it again
- This ensures clean state and accurate time tracking

#### No Target Task Specified
- Return error: "No tasks specified."
- Same behavior as `start` command

#### Multiple Target Tasks Match Filter
- Use same confirmation logic as `start` command
- Show affected tasks and ask for confirmation
- If confirmed, stop all active tasks, then start all matching tasks

#### Target Task is Completed/Deleted
- Same behavior as `start`: set status to pending first
- This is already handled by `CmdStart::execute()`

## Implementation Details

### Files to Create

#### `src/commands/CmdSwitch.h`
```cpp
#ifndef INCLUDED_CMDSWITCH
#define INCLUDED_CMDSWITCH

#include <Command.h>
#include <string>

class CmdSwitch : public Command {
 public:
  CmdSwitch();
  int execute(std::string&);
};

#endif
```

#### `src/commands/CmdSwitch.cpp`
- Implement `CmdSwitch::CmdSwitch()` constructor
- Implement `CmdSwitch::execute()` method
- Reuse logic from `CmdStart` and `CmdStop` where possible

### Files to Modify

#### `src/commands/Command.cpp`
- Add `#include <CmdSwitch.h>`
- Register command in `Command::factory()`:
  ```cpp
  c = new CmdSwitch();
  all[c->keyword()] = c;
  ```

#### `src/commands/CMakeLists.txt`
- Add `CmdSwitch.cpp` and `CmdSwitch.h` to the source list

#### `doc/man/task.1.in`
- Add documentation section for `switch` command
- Place after `stop` command documentation
- Include examples and edge case behavior

### Implementation Approach

#### Option 1: Direct Implementation (Recommended)
- Implement `CmdSwitch::execute()` directly
- Call `WorkInterval::log_event()` for stop/start events
- Reuse helper functions from `CmdStart` and `CmdStop` where possible
- **Pros**: Clear, explicit control flow
- **Cons**: Some code duplication

#### Option 2: Composition
- Call `CmdStop::execute()` for active tasks
- Call `CmdStart::execute()` for target task
- **Pros**: Reuses existing code
- **Cons**: Harder to control output/feedback, may have permission issues

**Recommendation**: Option 1 (Direct Implementation)

### Key Implementation Steps

1. **Query Active Tasks**
   ```cpp
   Filter activeFilter;
   activeFilter.addFilter("status:pending -WAITING +ACTIVE");
   std::vector<Task> activeTasks;
   activeFilter.subset(activeTasks);
   ```

2. **Stop Active Tasks**
   - For each active task:
     - Get stop time: `time_t stop_time = time(nullptr);`
     - Remove `start` attribute: `task.remove("start");`
     - Add annotation if journal.time enabled
     - Log work interval: `WorkInterval::log_event(task.get("uuid"), "stop", stop_time);`
     - Save task: `Context::getContext().tdb2.modify(task);`

3. **Start Target Task**
   - Apply filter to get target task(s)
   - Validate task exists and is not already active (if it's the only active task)
   - Set start time: `task.setAsNow("start");`
   - Handle status (completed/deleted -> pending)
   - Add annotation if journal.time enabled
   - Log work interval: `WorkInterval::log_event(task.get("uuid"), "start", start_time);`
   - Save task: `Context::getContext().tdb2.modify(task);`

4. **Feedback Messages**
   - Report how many tasks were stopped
   - Report which task was started
   - Use `feedback_affected()` for consistent messaging

### Command Configuration

```cpp
CmdSwitch::CmdSwitch() {
  _keyword = "switch";
  _usage = "task <filter> switch [<mods>]";
  _description = "Stops all active tasks and starts the specified task";
  _read_only = false;
  _displays_id = false;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = true;
  _accepts_filter = true;
  _accepts_modifications = true;
  _accepts_miscellaneous = false;  // No message support (unlike stop)
  _category = Command::Category::operation;
}
```

## Testing Strategy

### Unit Test Cases

1. **Basic Switch**
   - Task 5 is active
   - `task 10 switch`
   - Verify: Task 5 stopped, Task 10 started

2. **No Active Tasks**
   - No tasks active
   - `task 10 switch`
   - Verify: Task 10 started (same as `start`)

3. **Target Already Active (Only Task)**
   - Task 10 is the only active task
   - `task 10 switch`
   - Verify: No change, message "Task 10 is already active"

4. **Target Already Active (Multiple Active)**
   - Tasks 5 and 10 are active
   - `task 10 switch`
   - Verify: Tasks 5 and 10 stopped, Task 10 started again

5. **Multiple Active Tasks**
   - Tasks 5, 7, 9 are active
   - `task 10 switch`
   - Verify: All three stopped, Task 10 started

6. **Multiple Target Tasks**
   - Task 5 is active
   - `task project:Work switch` matches tasks 10, 11
   - Verify: Confirmation prompt, then Task 5 stopped, Tasks 10 and 11 started

7. **No Target Specified**
   - `task switch`
   - Verify: Error "No tasks specified."

8. **Target Task Completed**
   - Task 5 is active
   - Task 10 is completed
   - `task 10 switch`
   - Verify: Task 5 stopped, Task 10 status changed to pending and started

9. **Work Interval Logging**
   - Task 5 is active
   - `task 10 switch`
   - Verify: Stop event logged for Task 5, Start event logged for Task 10

10. **With Modifications**
    - Task 5 is active
    - `task 10 switch priority:H`
    - Verify: Task 5 stopped, Task 10 started with priority High

## Error Handling

### Invalid Filter
- Same behavior as `start`: "No tasks specified."

### Permission Denied
- If user denies permission during confirmation:
  - Stop operations that were already confirmed should complete
  - Start operation should be skipped
  - Return appropriate error code

### Work Interval Errors
- If work interval logging fails, log debug message but don't fail the command
- Same approach as `CmdStart` and `CmdStop`

## Documentation

### Man Page Entry
Add to `doc/man/task.1.in` after the `stop` command section:

```
.TP
.BR switch " [<mods>]"
Stops all currently active tasks and starts the specified task. This is a
convenience command that combines stopping all active tasks and starting a
new task into a single operation.

If no tasks are currently active, this command behaves identically to
.BR start "."

If the specified task is already the only active task, no changes are made
and a message is displayed.

Examples:
  task 10 switch
  task project:Work switch priority:H
```

### Help Text
The command description will appear in `task commands` output.

## Future Enhancements (Out of Scope)

1. **Configurable Behavior**: Allow configuration to control whether to re-start an already-active task
2. **Switch History**: Track which tasks were switched from/to
3. **Switch Reasons**: Optional message explaining why the switch occurred
4. **Batch Switching**: Support switching to multiple tasks simultaneously

## Dependencies

- Existing `CmdStart` and `CmdStop` implementations for reference
- `WorkInterval` class for logging interval events
- `Filter` class for querying active tasks
- `Task` class for task manipulation
- `TDB2` for saving task changes

## Implementation Checklist

- [ ] Create `src/commands/CmdSwitch.h`
- [ ] Create `src/commands/CmdSwitch.cpp`
- [ ] Register command in `src/commands/Command.cpp`
- [ ] Add to `src/commands/CMakeLists.txt`
- [ ] Add man page documentation in `doc/man/task.1.in`
- [ ] Test basic switch functionality
- [ ] Test edge cases (no active tasks, target already active, etc.)
- [ ] Test work interval logging
- [ ] Test with modifications
- [ ] Test error cases
- [ ] Verify feedback messages are clear
- [ ] Rebuild and test in real environment

## Notes

- The `switch` command should feel atomic - all stops happen, then the start happens
- If the start fails (e.g., permission denied), the stops should still have occurred
- This is intentional: the user explicitly requested to stop active tasks, so that should happen even if starting the new task fails
- Consider adding a configuration option `switch.keep-active-if-same` to control behavior when target is already the only active task
