// A thin C++ wrapper around SQLite's C API.
//
// SQLite hands out raw pointers that must be released by hand. These classes
// tie each pointer to an object's lifetime (RAII), so a connection or query is
// always cleaned up, even when an exception is thrown, and they turn SQLite's
// integer error codes into exceptions.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace ptt {

// Thrown when SQLite reports a failure (bad SQL, broken constraint, disk error).
class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// One prepared SQL statement. Values are bound to the ?1, ?2, ... placeholders
// instead of being pasted into the SQL text, which is what prevents SQL injection.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // Placeholders are numbered from 1. Each returns *this so calls can be chained.
    Statement& bindInt(int index, std::int64_t value);
    Statement& bindDouble(int index, double value);
    Statement& bindText(int index, const std::string& value);
    Statement& bindNull(int index);

    // Advances to the next result row. Returns false when there are no more rows.
    bool step();
    // Runs a statement that produces no rows (INSERT, UPDATE, DELETE).
    void run();

    // Columns of the current row are numbered from 0.
    std::int64_t columnInt(int column) const;
    double columnDouble(int column) const;
    std::string columnText(int column) const;
    bool columnIsNull(int column) const;

private:
    void check(int resultCode) const;

    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

// An open connection to one database file.
class Database {
public:
    // Opens the file, creating it if needed. Pass ":memory:" for a temporary
    // database that lives only as long as this object (used by the tests).
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Statement prepare(const std::string& sql);
    // Runs one or more statements that take no parameters and return no rows.
    void execute(const std::string& sql);

    std::int64_t lastInsertId() const;
    // Number of rows changed by the most recent INSERT, UPDATE or DELETE.
    int changes() const;

private:
    sqlite3* db_ = nullptr;
};

// Groups several changes so they all happen or none do. If commit() is never
// reached (for example because an exception was thrown), the destructor rolls
// everything back.
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();

private:
    Database& db_;
    bool finished_ = false;
};

}  // namespace ptt
