#include "db_migration.h"
#include "../properties.h"
#include "../static.h"
#include "assert2.h"
#include "logger.h"
#include <dirent.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define DB_MIGRATE_DIR "migrations/"
#define DB_MIGRATION_MAX_FILE_SIZE (PATH_MAX)
#define DB_MIGRATE_BUFFER_SIZE 1 << 16
#define SCHEMA_VERSION_SQL                                                     \
  "SELECT version, name FROM SchemaVersion ORDER BY version DESC;"
#define SCHEMA_VERSION_INSERT_SQL                                              \
  "INSERT INTO SchemaVersion (version, name) VALUES (?, ?);"

static sqlite3 *db = NULL;
static sqlite3_stmt *stmt = NULL;
static char migration_files[10][DB_MIGRATION_MAX_FILE_SIZE];
static unsigned char migration_files_count = 0;

static inline int get_current_schema_version() {
  assert(db != NULL);
  assert(stmt == NULL);

  if (sqlite3_prepare_v2(db, SCHEMA_VERSION_SQL, -1, &stmt, NULL) !=
      SQLITE_OK) {
    const char *err_msg = sqlite3_errmsg(db);
    if (err_msg != NULL && strstr(err_msg, "no such table") != NULL) {
      return 0;
    } else {
      log_error("Failed to prepare statement: %s", err_msg);
      sqlite3_close(db);
      return -1;
    }
  }
  int last_version = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int version = sqlite3_column_int(stmt, 0);
    if (version > last_version)
      last_version = version;
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    log_debug("Already migrated version: %d, name: %s", version, name);
  }
  sqlite3_finalize(stmt);
  return last_version;
}

static inline int insert_schema_version(int version, const char *name) {
  assert(db != NULL);

  if (sqlite3_prepare_v2(db, SCHEMA_VERSION_INSERT_SQL, -1, &stmt, NULL) !=
      SQLITE_OK) {
    log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_bind_int(stmt, 1, version);
  sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    log_error("Failed to insert schema version: %s", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

static inline int begin_transaction() {
  char *err_msg = NULL;
  int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
  if (err_msg != NULL) {
    log_error("Failed to begin transaction: %s", err_msg);
    sqlite3_free(err_msg);
  }
  return rc;
}

static inline int commit() {
  char *err_msg = NULL;
  int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &err_msg);
  if (err_msg != NULL) {
    log_error("Failed to commit transaction: %s", err_msg);
    sqlite3_free(err_msg);
  }
  return rc;
}

static inline int rollback() {
  char *err_msg = NULL;
  int rc = sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
  if (err_msg != NULL) {
    log_error("Failed to rollback transaction: %s", err_msg);
    sqlite3_free(err_msg);
  }
  return rc;
}

static inline int find_migration_files(int current_version) {
  // Open migration directory
  DIR *dir = opendir(DB_MIGRATE_DIR);
  if (dir == NULL) {
    log_error("Failed to open directory: %s", DB_MIGRATE_DIR);
    return -1;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    int version;
    // Skip if not a regular file or doesn't match the version pattern
    if (entry->d_type != DT_REG ||
        sscanf(entry->d_name, "v%d_", &version) != 1 ||
        version <= current_version) {
      continue;
    }

    strcpy(migration_files[migration_files_count], entry->d_name);
    migration_files_count++;
  }

  if (migration_files_count > 0)
    qsort(migration_files, migration_files_count, sizeof(migration_files[0]),
          (int (*)(const void *, const void *))strcmp);

  closedir(dir);
  return 0;
}

static inline int clean_db() {
  assert(db == NULL);

  return remove(get_db_url());
}

int migrate_db() {
  if (get_reset_db() == 1) {
    log_info("Resetting database");
    if (clean_db() != 0) {
      log_error("Failed to reset database");
      return -1;
    }
  }

  log_info("Starting database migration in directory: %s", DB_MIGRATE_DIR);
  char buffer[(DB_MIGRATE_BUFFER_SIZE) + 1],
      full_path[DB_MIGRATION_MAX_FILE_SIZE];
  char *err_msg = NULL;

  // Open SQLite database
  if (sqlite3_open_v2(get_db_url(), &db, WB_SQLITE_OPEN_FLAGS, NULL) !=
      SQLITE_OK) {
    log_error("Failed to open database: %s", sqlite3_errmsg(db));
    return -1;
  }

  // Get current schema version
  int current_version = get_current_schema_version();
  if (current_version < 0) {
    log_error("Failed to get current schema version");
    sqlite3_close(db);
    return -1;
  }

  if (find_migration_files(current_version) != 0) {
    log_error("Failed to find migration files");
    sqlite3_close(db);
    return -1;
  }

  // Begin transaction
  if (begin_transaction() != SQLITE_OK) {
    sqlite3_close(db);
    return -1;
  }

  for (int i = 0; i < migration_files_count; i++) {
    log_debug("Migrating: %s", migration_files[i]);

    if (snprintf(full_path, DB_MIGRATION_MAX_FILE_SIZE, "%s%s", DB_MIGRATE_DIR,
                 migration_files[i]) >= DB_MIGRATION_MAX_FILE_SIZE) {
      log_error("Migration file path is too long: %s", migration_files[i]);
      rollback();
      sqlite3_close(db);
      return -1;
    }

    FILE *file = fopen(full_path, "r");
    if (file == NULL) {
      log_error("Failed to open migration file: %s", migration_files[i]);
      rollback();
      sqlite3_close(db);
      return -1;
    }

    unsigned int read_size = fread(buffer, 1, DB_MIGRATE_BUFFER_SIZE, file);
    if (read_size == 0) {
      log_error("Failed to read migration file: %s", migration_files[i]);
      rollback();
      sqlite3_close(db);
      fclose(file);
      return -1;
    }
    buffer[read_size] = '\0';
    // log_trace("Migration file content:\n%s", buffer);
    if (sqlite3_exec(db, buffer, NULL, NULL, &err_msg) != SQLITE_OK) {
      log_error("Failed to execute migration file: %s. Error: %s",
                migration_files[i], err_msg);
      rollback();
      sqlite3_free(err_msg);
      sqlite3_close(db);
      fclose(file);
      return -1;
    }
    if (insert_schema_version(current_version + 1, migration_files[i]) != 0) {
      log_error("Failed to insert schema version for: %s", migration_files[i]);
      rollback();
      fclose(file);
      sqlite3_close(db);
      return -1;
    }

    current_version++;
    fclose(file);
  }

  // Commit transaction
  commit();

  sqlite3_close(db);
  return 0;
}
