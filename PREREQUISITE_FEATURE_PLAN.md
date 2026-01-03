# Prerequisite Feature Implementation Plan

## Overview

Add a `prerequisite:` attribute that allows marking a subtask as a prerequisite for a main task. This is a reverse dependency syntax that complements the existing `depends:` feature.

**Current workflow:**
1. Create subtask1, subtask2
2. Create maintask with `depends:subtask1,subtask2`

**New workflow:**
1. Create maintask (id=5)
2. Create subtask1 with `prerequisite:5` (id=6)
3. System automatically adds `depends:6` to maintask (id=5)

## Syntax

```
prerequisite:<id1,id2 ...>
```

- Accepts comma-separated list of task IDs, UUIDs, or ID ranges
- When prefixing any element with `-`, removes the prerequisite relationship
- Supports the same formats as `depends:` (ID, UUID, ID ranges)

## Implementation Details

### 1. Create ColumnPrerequisite Class

**File:** `src/columns/ColPrerequisite.h` and `src/columns/ColPrerequisite.cpp`

- Inherit from `ColumnTypeString` (similar to `ColumnDepends`)
- Include `<util.h>` for `uuid()` function
- Implement `modify()` method that:
  1. Parses the prerequisite value (supports ID, UUID, ID ranges, comma-separated lists)
  2. Resolves each target task ID/UUID to the actual task
  3. For each target task, adds the current task as a dependency
  4. Handles removal with `-` prefix
  5. Validates that target tasks exist
  6. Ensures current task has a UUID (should be assigned during `validate()`)

**Key differences from `ColumnDepends::modify()`:**
- Instead of `task.addDependency(target)`, we do `target_task.addDependency(current_task_uuid)`
- Need to load and modify the target task(s), not just the current task
- Need to save the modified target task(s) back to the database
- **Important:** Must ensure current task has a UUID before processing. For new tasks, UUID may not be assigned yet (it's assigned in `validate()` which runs after `modify()`). Solution: Call `task.validate(false)` or ensure UUID exists before processing prerequisites.

### 2. Register ColumnPrerequisite

**File:** `src/columns/Column.cpp`

- Add `#include <ColPrerequisite.h>`
- Add case in `Column::factory(const std::string& name, ...)`:
  ```cpp
  else if (column_name == "prerequisite")
    c = new ColumnPrerequisite();
  ```
- Add instantiation in `Column::factory(std::map<std::string, Column*>& all)`:
  ```cpp
  c = new ColumnPrerequisite();
  all[c->_name] = c;
  ```

### 3. Update CMakeLists.txt

**File:** `src/columns/CMakeLists.txt`

- Add `ColPrerequisite.cpp` and `ColPrerequisite.h` to the source files list

### 4. Handle Task Saving

**Challenge:** When `prerequisite:5` is set on task 6, we need to:
1. Load task 5
2. Add task 6's UUID to task 5's dependencies
3. Save task 5 back to the database

**Solution:** In `ColumnPrerequisite::modify()`, after modifying target tasks:
- Call `Context::getContext().tdb2.modify(target_task)` for each modified target task
- This ensures the dependency is persisted immediately

**File:** `src/columns/ColPrerequisite.cpp`

```cpp
void ColumnPrerequisite::modify(Task& task, const std::string& value) {
  // Ensure current task has a UUID (for new tasks, validate assigns UUID)
  // Note: validate() is typically called after modify(), but we need UUID now
  // So we ensure it exists here
  if (!task.has("uuid") || task.get("uuid") == "") {
    // For new tasks, assign UUID now (validate() will check it later)
    task.set("uuid", uuid());
  }
  std::string current_uuid = task.get("uuid");
  
  // Parse prerequisite list (similar to ColumnDepends::modify)
  for (auto& prereq : split(value, ',')) {
    bool removal = false;
    if (prereq[0] == '-') {
      removal = true;
      prereq = prereq.substr(1);
    }
    
    // Resolve to target task(s) - support ID, UUID, ID ranges
    // ... (similar parsing logic to ColumnDepends)
    
    // For each target task:
    Task target_task;
    if (/* resolve to target task */) {
      if (removal) {
        target_task.removeDependency(current_uuid);
      } else {
        target_task.addDependency(current_uuid);
      }
      
      // Save the modified target task
      Context::getContext().tdb2.modify(target_task);
    }
  }
}
```

### 5. Circular Dependency Detection

**Existing mechanism:** `Task::addDependency()` already calls `dependencyIsCircular()` which uses DFS to detect cycles.

**Consideration:** When we add task 6 as a dependency to task 5 (via `prerequisite:5` on task 6), the circular dependency check will run on task 5. This should work correctly because:
- Task 5's `addDependency(task6_uuid)` will check if adding task 6 creates a cycle
- The check traverses task 5's dependency chain, which may include task 6 if there's already a reverse dependency

**No additional changes needed** - existing circular dependency detection should handle this correctly.

### 6. Edge Cases and Validation

#### 6.1 New Task Without UUID
- **Scenario:** User creates a new task with `prerequisite:5`
- **Solution:** UUID is assigned during `Task::validate()` which runs before column modifications
- **Validation:** Add a check in `ColumnPrerequisite::modify()` to ensure UUID exists (defensive programming)

#### 6.2 Target Task Doesn't Exist
- **Scenario:** `prerequisite:999` where task 999 doesn't exist
- **Solution:** Use same error handling as `ColumnDepends::modify()` - throw an exception with a clear message

#### 6.3 Self-Reference
- **Scenario:** `prerequisite:5` on task 5 itself
- **Solution:** `Task::addDependency()` already checks for self-dependency and throws: "A task cannot be dependent on itself."

#### 6.4 Multiple Prerequisites
- **Scenario:** `prerequisite:5,6,7`
- **Solution:** Parse comma-separated list and process each one

#### 6.5 ID Ranges
- **Scenario:** `prerequisite:5-10`
- **Solution:** Support ID ranges (same logic as `ColumnDepends::modify()`)

#### 6.6 Removal
- **Scenario:** `prerequisite:-5` or `prerequisite:5,-6`
- **Solution:** Support `-` prefix for removal (same as `depends:`)

#### 6.7 Concurrent Modifications
- **Scenario:** Two tasks simultaneously set `prerequisite:5`
- **Solution:** Same race condition handling as existing `depends:` - TDB2 handles this at the database level

### 7. Documentation

**File:** `doc/man/task.1.in`

Add documentation for the `prerequisite:` attribute, similar to `depends:`:

```
.B prerequisite:<id1,id2 ...>
Declares this task to be a prerequisite for id1 and id2.  This means that
this task should be completed before tasks id1 and id2.  This is the reverse
of the depends: attribute - when you set prerequisite:5 on a task, it
automatically adds this task to task 5's dependency list.  It accepts a
comma-separated list of ID numbers, UUID numbers and ID ranges.  When
prefixing any element of this list by '-', the specified tasks are removed
from the prerequisite list.
```

### 8. Testing Considerations

#### 8.1 Basic Functionality
- Create maintask (id=5)
- Create subtask with `prerequisite:5`
- Verify maintask has `depends:subtask_uuid`

#### 8.2 Multiple Prerequisites
- Create maintask (id=5)
- Create subtask1 with `prerequisite:5`
- Create subtask2 with `prerequisite:5`
- Verify maintask has both subtasks in dependencies

#### 8.3 Removal
- Set up prerequisite relationship
- Remove with `prerequisite:-5`
- Verify dependency is removed from target task

#### 8.4 Circular Dependency
- Task 5 depends on task 6
- Try to set `prerequisite:5` on task 6
- Should fail with circular dependency error

#### 8.5 ID Ranges
- Create maintask (id=5)
- Create subtasks with `prerequisite:5-7`
- Verify all three maintasks have the subtask in dependencies

#### 8.6 Non-existent Task
- Try `prerequisite:999` where 999 doesn't exist
- Should fail with clear error message

## Implementation Order

1. **Create ColumnPrerequisite class** (`ColPrerequisite.h` and `.cpp`)
   - Copy structure from `ColumnDepends`
   - Implement `modify()` with reverse dependency logic
   - Add proper error handling

2. **Register the column** (`Column.cpp`)
   - Add include
   - Add factory cases

3. **Update build system** (`CMakeLists.txt`)
   - Add source files

4. **Test basic functionality**
   - Create simple test cases
   - Verify dependencies are created correctly

5. **Add documentation** (`task.1.in`)
   - Document syntax and usage

6. **Test edge cases**
   - Circular dependencies
   - Removal
   - ID ranges
   - Error cases

## Files to Modify

1. `src/columns/ColPrerequisite.h` (new)
2. `src/columns/ColPrerequisite.cpp` (new)
3. `src/columns/Column.cpp` (modify)
4. `src/columns/CMakeLists.txt` (modify)
5. `doc/man/task.1.in` (modify)

## Files to Reference

- `src/columns/ColDepends.h` - Structure reference
- `src/columns/ColDepends.cpp` - Implementation reference
- `src/Task.cpp` - `addDependency()`, `removeDependency()` methods
- `src/dependency.cpp` - Circular dependency detection
- `src/TDB2.cpp` - Task database operations
- `src/util.h` / `src/util.cpp` - `uuid()` function for generating UUIDs

## Notes

- The `prerequisite:` attribute is **not stored** in the task data - it's a virtual attribute that immediately translates to `depends:` on the target task(s)
- This means `prerequisite:` won't appear in task exports or JSON - only the resulting `depends:` relationships will be visible
- The feature is purely syntactic sugar for the reverse dependency workflow
- Both `depends:` and `prerequisite:` can coexist - they're just different ways to express the same relationship
