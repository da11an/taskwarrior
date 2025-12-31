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

#include <WorkInterval.h>
#include <Context.h>
#include <FS.h>
#include <format.h>
#include <shared.h>

#include <sqlite3.h>
#include <cstring>
#include <sstream>
#include <stdexcept>

// Static member initialization
bool WorkInterval::_initialized = false;
std::string WorkInterval::_db_path = "";

////////////////////////////////////////////////////////////////////////////////
std::string WorkInterval::get_db_path() {
  if (!_db_path.empty()) {
    return _db_path;
  }

  // Get database path from Context
  // Database is at {data.location}/taskchampion.sqlite3
  std::string data_location = Context::getContext().data_dir._data;
  _db_path = format("{1}/taskchampion.sqlite3", data_location);
  return _db_path;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::initialize(const std::string& db_path) {
  _db_path = db_path;
  ensure_table_exists();
  _initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
bool WorkInterval::table_exists() {
  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READONLY, nullptr);
  if (rc != SQLITE_OK) {
    // Database might not exist yet, that's okay
    if (db) sqlite3_close(db);
    return false;
  }

  // Check if table exists
  const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='work_intervals';";
  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  bool exists = false;
  if (rc == SQLITE_OK) {
    rc = sqlite3_step(stmt);
    exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  return exists;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::create_table() {
  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  if (rc != SQLITE_OK) {
    std::string error = format("Cannot open database: {1}", sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    throw std::runtime_error(error);
  }

  const char* sql =
      "CREATE TABLE IF NOT EXISTS work_intervals ("
      "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "    task_uuid TEXT NOT NULL,"
      "    event_type TEXT NOT NULL,"
      "    timestamp INTEGER NOT NULL,"
      "    message TEXT,"
      "    interval_id INTEGER,"
      "    created_at INTEGER NOT NULL"
      ");"
      "CREATE INDEX IF NOT EXISTS idx_work_intervals_task_uuid ON work_intervals(task_uuid);"
      "CREATE INDEX IF NOT EXISTS idx_work_intervals_timestamp ON work_intervals(timestamp);"
      "CREATE INDEX IF NOT EXISTS idx_work_intervals_interval_id ON work_intervals(interval_id);";

  char* errmsg = nullptr;
  rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);

  if (rc != SQLITE_OK) {
    std::string error = format("Cannot create work_intervals table: {1}", errmsg ? errmsg : "unknown error");
    sqlite3_free(errmsg);
    sqlite3_close(db);
    throw std::runtime_error(error);
  }

  sqlite3_close(db);
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::ensure_table_exists() {
  if (!table_exists()) {
    create_table();
  }
}

////////////////////////////////////////////////////////////////////////////////
int WorkInterval::get_next_interval_id(const std::string& task_uuid) {
  ensure_table_exists();

  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READWRITE, nullptr);
  if (rc != SQLITE_OK) {
    std::string error = format("Cannot open database: {1}", sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    throw std::runtime_error(error);
  }

  // Get the maximum interval_id for this task, or 0 if none exists
  const char* sql = "SELECT MAX(interval_id) FROM work_intervals WHERE task_uuid = ?;";
  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  int next_id = 1;
  if (rc == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, task_uuid.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      int max_id = sqlite3_column_int(stmt, 0);
      if (max_id > 0) {
        // Check if there's an open interval (start without stop/done)
        const char* check_sql =
            "SELECT COUNT(*) FROM work_intervals "
            "WHERE task_uuid = ? AND interval_id = ? AND event_type = 'start' "
            "AND interval_id NOT IN ("
            "  SELECT interval_id FROM work_intervals "
            "  WHERE task_uuid = ? AND interval_id = ? AND event_type IN ('stop', 'done')"
            ");";
        sqlite3_stmt* check_stmt = nullptr;
        int check_rc = sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr);
        if (check_rc == SQLITE_OK) {
          sqlite3_bind_text(check_stmt, 1, task_uuid.c_str(), -1, SQLITE_STATIC);
          sqlite3_bind_int(check_stmt, 2, max_id);
          sqlite3_bind_text(check_stmt, 3, task_uuid.c_str(), -1, SQLITE_STATIC);
          sqlite3_bind_int(check_stmt, 4, max_id);
          check_rc = sqlite3_step(check_stmt);
          if (check_rc == SQLITE_ROW) {
            int open_count = sqlite3_column_int(check_stmt, 0);
            if (open_count == 0) {
              // Last interval is closed, use next ID
              next_id = max_id + 1;
            } else {
              // Last interval is still open, reuse the ID
              next_id = max_id;
            }
          }
          sqlite3_finalize(check_stmt);
        }
      }
    }
    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  return next_id;
}

////////////////////////////////////////////////////////////////////////////////
void WorkInterval::log_event(const std::string& task_uuid,
                             const std::string& event_type,
                             time_t timestamp,
                             const std::string& message) {
  ensure_table_exists();

  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READWRITE, nullptr);
  if (rc != SQLITE_OK) {
    std::string error = format("Cannot open database: {1}", sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    throw std::runtime_error(error);
  }

  // Determine interval_id
  int interval_id = 0;
  if (event_type == "start") {
    interval_id = get_next_interval_id(task_uuid);
  } else if (event_type == "stop" || event_type == "done") {
    // Find the most recent open interval (start without stop/done)
    const char* find_sql =
        "SELECT interval_id FROM work_intervals "
        "WHERE task_uuid = ? AND event_type = 'start' "
        "AND interval_id NOT IN ("
        "  SELECT interval_id FROM work_intervals "
        "  WHERE task_uuid = ? AND event_type IN ('stop', 'done')"
        ") "
        "ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, find_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, task_uuid.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, task_uuid.c_str(), -1, SQLITE_STATIC);
      rc = sqlite3_step(stmt);
      if (rc == SQLITE_ROW) {
        interval_id = sqlite3_column_int(stmt, 0);
      }
      sqlite3_finalize(stmt);
    }
    // If no open interval found, create a new one (shouldn't happen in normal flow)
    if (interval_id == 0) {
      interval_id = get_next_interval_id(task_uuid);
    }
  }

  // Insert the event
  const char* insert_sql =
      "INSERT INTO work_intervals (task_uuid, event_type, timestamp, message, interval_id, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);

  if (rc == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, task_uuid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, event_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(timestamp));
    if (message.empty()) {
      sqlite3_bind_null(stmt, 4);
    } else {
      sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 5, interval_id);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(time(nullptr)));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      std::string error = format("Cannot insert work interval: {1}", sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);
  } else {
    std::string error = format("Cannot prepare insert statement: {1}", sqlite3_errmsg(db));
    sqlite3_close(db);
    throw std::runtime_error(error);
  }

  sqlite3_close(db);
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Interval> WorkInterval::get_intervals(const std::string& task_uuid) {
  ensure_table_exists();

  std::vector<Interval> intervals;
  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READONLY, nullptr);
  if (rc != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return intervals;  // Return empty on error
  }

  // Query for completed intervals (start + stop/done pairs)
  const char* sql =
      "SELECT start.timestamp, end.timestamp, end.event_type, end.message, start.interval_id "
      "FROM work_intervals start "
      "JOIN work_intervals end ON start.interval_id = end.interval_id AND start.task_uuid = end.task_uuid "
      "WHERE start.task_uuid = ? "
      "  AND start.event_type = 'start' "
      "  AND end.event_type IN ('stop', 'done') "
      "ORDER BY start.timestamp ASC;";

  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  if (rc == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, task_uuid.c_str(), -1, SQLITE_STATIC);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      Interval interval;
      interval.task_uuid = task_uuid;
      interval.start_time = static_cast<time_t>(sqlite3_column_int64(stmt, 0));
      interval.end_time = static_cast<time_t>(sqlite3_column_int64(stmt, 1));
      const unsigned char* event_type = sqlite3_column_text(stmt, 2);
      interval.event_type = event_type ? reinterpret_cast<const char*>(event_type) : "";
      const unsigned char* message = sqlite3_column_text(stmt, 3);
      interval.message = message ? reinterpret_cast<const char*>(message) : "";
      interval.interval_id = sqlite3_column_int(stmt, 4);
      intervals.push_back(interval);
    }
    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  return intervals;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Interval> WorkInterval::get_intervals_by_date(time_t start_time,
                                                          time_t end_time) {
  ensure_table_exists();

  std::vector<Interval> intervals;
  sqlite3* db = nullptr;
  std::string db_path = get_db_path();

  int rc = sqlite3_open_v2(db_path.c_str(), &db,
                           SQLITE_OPEN_READONLY, nullptr);
  if (rc != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return intervals;  // Return empty on error
  }

  // Query for completed intervals within date range
  const char* sql =
      "SELECT start.task_uuid, start.timestamp, end.timestamp, end.event_type, end.message, start.interval_id "
      "FROM work_intervals start "
      "JOIN work_intervals end ON start.interval_id = end.interval_id AND start.task_uuid = end.task_uuid "
      "WHERE start.timestamp >= ? AND start.timestamp <= ? "
      "  AND start.event_type = 'start' "
      "  AND end.event_type IN ('stop', 'done') "
      "ORDER BY start.timestamp ASC;";

  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  if (rc == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(start_time));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(end_time));
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      Interval interval;
      const unsigned char* uuid = sqlite3_column_text(stmt, 0);
      interval.task_uuid = uuid ? reinterpret_cast<const char*>(uuid) : "";
      interval.start_time = static_cast<time_t>(sqlite3_column_int64(stmt, 1));
      interval.end_time = static_cast<time_t>(sqlite3_column_int64(stmt, 2));
      const unsigned char* event_type = sqlite3_column_text(stmt, 3);
      interval.event_type = event_type ? reinterpret_cast<const char*>(event_type) : "";
      const unsigned char* message = sqlite3_column_text(stmt, 4);
      interval.message = message ? reinterpret_cast<const char*>(message) : "";
      interval.interval_id = sqlite3_column_int(stmt, 5);
      intervals.push_back(interval);
    }
    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  return intervals;
}

////////////////////////////////////////////////////////////////////////////////
