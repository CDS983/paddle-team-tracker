#include "database.h"

#include <sqlite3.h>

#include <utility>

namespace ptt {

// ---- Statement ----

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw DatabaseError(std::string("Could not prepare SQL: ") + sqlite3_errmsg(db_) + "\n" + sql);
    }
}

Statement::~Statement() {
    sqlite3_finalize(stmt_);  // safe to call with nullptr
}

Statement::Statement(Statement&& other) noexcept
    : db_(other.db_), stmt_(std::exchange(other.stmt_, nullptr)) {}

void Statement::check(int resultCode) const {
    if (resultCode != SQLITE_OK) throw DatabaseError(sqlite3_errmsg(db_));
}

Statement& Statement::bindInt(int index, std::int64_t value) {
    check(sqlite3_bind_int64(stmt_, index, value));
    return *this;
}

Statement& Statement::bindDouble(int index, double value) {
    check(sqlite3_bind_double(stmt_, index, value));
    return *this;
}

Statement& Statement::bindText(int index, const std::string& value) {
    // SQLITE_TRANSIENT tells SQLite to keep its own copy of the text, so the
    // caller's string does not have to outlive this call.
    check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    return *this;
}

Statement& Statement::bindNull(int index) {
    check(sqlite3_bind_null(stmt_, index));
    return *this;
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DatabaseError(sqlite3_errmsg(db_));
}

void Statement::run() {
    while (step()) {
        // Ignore any rows; run() is for statements that are not expected to return them.
    }
}

std::int64_t Statement::columnInt(int column) const {
    return sqlite3_column_int64(stmt_, column);
}

double Statement::columnDouble(int column) const {
    return sqlite3_column_double(stmt_, column);
}

std::string Statement::columnText(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    if (text == nullptr) return {};
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(sqlite3_column_bytes(stmt_, column)));
}

bool Statement::columnIsNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

// ---- Database ----

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw DatabaseError("Could not open " + path + ": " + message);
    }
    // SQLite ignores FOREIGN KEY rules unless each connection switches them on.
    execute("PRAGMA foreign_keys = ON");
}

Database::~Database() {
    sqlite3_close(db_);
}

Statement Database::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

void Database::execute(const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw DatabaseError(message);
    }
}

std::int64_t Database::lastInsertId() const {
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const {
    return sqlite3_changes(db_);
}

// ---- Transaction ----

Transaction::Transaction(Database& db) : db_(db) {
    db_.execute("BEGIN");
}

Transaction::~Transaction() {
    if (finished_) return;
    try {
        db_.execute("ROLLBACK");
    } catch (const DatabaseError&) {
        // A destructor must not throw. If the rollback itself fails there is
        // nothing more that can be done here.
    }
}

void Transaction::commit() {
    db_.execute("COMMIT");
    finished_ = true;
}

}  // namespace ptt
