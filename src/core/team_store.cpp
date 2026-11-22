#include "team_store.h"

#include "format.h"

namespace ptt {

namespace {

// Bump this and add a step to createSchema() whenever the tables change.
const int kSchemaVersion = 1;

// The athlete columns in the order readAthlete() expects them.
const std::string kAthleteColumns = "a.id, a.name, a.side, a.role, a.gender, a.weight, a.notes, a.active";

Athlete readAthlete(const Statement& row, int first = 0) {
    Athlete athlete;
    athlete.id = row.columnInt(first);
    athlete.name = row.columnText(first + 1);
    athlete.side = sideFromCode(row.columnText(first + 2)).value_or(Side::Both);
    athlete.role = roleFromCode(row.columnText(first + 3)).value_or(Role::Paddler);
    athlete.gender = row.columnText(first + 4);
    if (!row.columnIsNull(first + 5)) athlete.weight = row.columnDouble(first + 5);
    athlete.notes = row.columnText(first + 6);
    athlete.active = row.columnInt(first + 7) != 0;
    return athlete;
}

Test readTest(const Statement& row, int first = 0) {
    Test test;
    test.id = row.columnInt(first);
    test.name = row.columnText(first + 1);
    test.kind = row.columnText(first + 2) == "number" ? TestKind::Number : TestKind::Time;
    test.unit = row.columnText(first + 3);
    test.lowerIsBetter = row.columnInt(first + 4) != 0;
    return test;
}

// Checks an athlete before saving and returns a tidied copy.
Athlete validated(Athlete athlete) {
    athlete.name = trim(athlete.name);
    athlete.notes = trim(athlete.notes);
    if (athlete.name.empty()) throw ValidationError("Enter a name.");
    if (athlete.name.size() > 80) throw ValidationError("Keep the name under 80 characters.");
    if (athlete.gender != "" && athlete.gender != "F" && athlete.gender != "M") {
        throw ValidationError("Gender must be F, M or left blank.");
    }
    if (athlete.weight && (*athlete.weight <= 0 || *athlete.weight > 999)) {
        throw ValidationError("Weight must be between 1 and 999.");
    }
    return athlete;
}

void bindAthlete(Statement& statement, const Athlete& athlete) {
    statement.bindText(1, athlete.name)
        .bindText(2, code(athlete.side))
        .bindText(3, code(athlete.role))
        .bindText(4, athlete.gender);
    if (athlete.weight) statement.bindDouble(5, *athlete.weight); else statement.bindNull(5);
    statement.bindText(6, athlete.notes).bindInt(7, athlete.active ? 1 : 0);
}

void requireDate(const std::string& date) {
    if (!isValidDate(date)) throw ValidationError("Enter the date as YYYY-MM-DD, for example 2026-03-14.");
}

}  // namespace

TeamStore::TeamStore(Database& db) : db_(db) {
    createSchema();
}

// ---------------------------------------------------------------------------
// Schema
// ---------------------------------------------------------------------------

void TeamStore::createSchema() {
    // SQLite keeps one spare integer per database file, "user_version".
    // It is used here to remember which version of the tables the file has.
    Statement query = db_.prepare("PRAGMA user_version");
    query.step();
    const std::int64_t version = query.columnInt(0);
    if (version >= kSchemaVersion) return;

    Transaction transaction(db_);
    db_.execute(R"sql(
        CREATE TABLE settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        CREATE TABLE athletes (
            id     INTEGER PRIMARY KEY,
            name   TEXT    NOT NULL,
            side   TEXT    NOT NULL CHECK (side IN ('L', 'R', 'B')),
            role   TEXT    NOT NULL CHECK (role IN ('paddler', 'drummer', 'steer')),
            gender TEXT    NOT NULL DEFAULT '' CHECK (gender IN ('', 'F', 'M')),
            weight REAL,
            notes  TEXT    NOT NULL DEFAULT '',
            active INTEGER NOT NULL DEFAULT 1
        );

        CREATE TABLE practices (
            id    INTEGER PRIMARY KEY,
            date  TEXT NOT NULL,
            title TEXT NOT NULL DEFAULT '',
            notes TEXT NOT NULL DEFAULT ''
        );

        -- One row per athlete per practice. The two-column primary key means an
        -- athlete can only have one mark at a practice. ON DELETE CASCADE removes
        -- the marks automatically when the athlete or the practice is deleted.
        CREATE TABLE attendance (
            practice_id INTEGER NOT NULL REFERENCES practices(id) ON DELETE CASCADE,
            athlete_id  INTEGER NOT NULL REFERENCES athletes(id)  ON DELETE CASCADE,
            status      TEXT    NOT NULL CHECK (status IN ('P', 'L', 'A', 'E')),
            PRIMARY KEY (practice_id, athlete_id)
        );

        CREATE TABLE tests (
            id              INTEGER PRIMARY KEY,
            name            TEXT    NOT NULL,
            kind            TEXT    NOT NULL CHECK (kind IN ('time', 'number')),
            unit            TEXT    NOT NULL DEFAULT '',
            lower_is_better INTEGER NOT NULL DEFAULT 1
        );

        CREATE TABLE results (
            id         INTEGER PRIMARY KEY,
            athlete_id INTEGER NOT NULL REFERENCES athletes(id) ON DELETE CASCADE,
            test_id    INTEGER NOT NULL REFERENCES tests(id)    ON DELETE CASCADE,
            date       TEXT    NOT NULL,
            value      REAL    NOT NULL CHECK (value >= 0)
        );

        -- Rankings and athlete pages look results up by test and by athlete.
        CREATE INDEX results_by_test_athlete ON results (test_id, athlete_id);
    )sql");
    addDefaultTests();
    db_.execute("PRAGMA user_version = " + std::to_string(kSchemaVersion));
    transaction.commit();
}

void TeamStore::addDefaultTests() {
    addTest("200 m time trial", TestKind::Time);
    addTest("500 m time trial", TestKind::Time);
    addTest("2000 m time trial", TestKind::Time);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

std::string TeamStore::setting(const std::string& key, const std::string& fallback) {
    Statement query = db_.prepare("SELECT value FROM settings WHERE key = ?1");
    query.bindText(1, key);
    return query.step() ? query.columnText(0) : fallback;
}

void TeamStore::setSetting(const std::string& key, const std::string& value) {
    // "Upsert": insert the row, or update it if the key is already there.
    db_.prepare("INSERT INTO settings (key, value) VALUES (?1, ?2) "
                "ON CONFLICT (key) DO UPDATE SET value = excluded.value")
        .bindText(1, key).bindText(2, value).run();
}

std::string TeamStore::teamName() {
    return setting("team_name", "My crew");
}

void TeamStore::setTeamName(const std::string& name) {
    const std::string cleaned = trim(name);
    if (cleaned.empty()) throw ValidationError("Enter a team name.");
    setSetting("team_name", cleaned);
}

std::string TeamStore::weightUnit() {
    return setting("weight_unit", "lb");
}

void TeamStore::setWeightUnit(const std::string& unit) {
    if (unit != "lb" && unit != "kg") throw ValidationError("The weight unit must be lb or kg.");
    setSetting("weight_unit", unit);
}

// ---------------------------------------------------------------------------
// Athletes
// ---------------------------------------------------------------------------

std::int64_t TeamStore::addAthlete(const Athlete& athlete) {
    const Athlete clean = validated(athlete);
    Statement insert = db_.prepare(
        "INSERT INTO athletes (name, side, role, gender, weight, notes, active) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    bindAthlete(insert, clean);
    insert.run();
    return db_.lastInsertId();
}

void TeamStore::updateAthlete(const Athlete& athlete) {
    const Athlete clean = validated(athlete);
    Statement update = db_.prepare(
        "UPDATE athletes SET name = ?1, side = ?2, role = ?3, gender = ?4, weight = ?5, notes = ?6, active = ?7 WHERE id = ?8");
    bindAthlete(update, clean);
    update.bindInt(8, clean.id).run();
}

void TeamStore::deleteAthlete(std::int64_t id) {
    db_.prepare("DELETE FROM athletes WHERE id = ?1").bindInt(1, id).run();
}

std::optional<Athlete> TeamStore::athlete(std::int64_t id) {
    Statement query = db_.prepare("SELECT " + kAthleteColumns + " FROM athletes a WHERE a.id = ?1");
    query.bindInt(1, id);
    if (!query.step()) return std::nullopt;
    return readAthlete(query);
}

std::vector<Athlete> TeamStore::athletes(bool includeInactive) {
    // COLLATE NOCASE sorts "ana" next to "Ana" instead of after "Zed".
    Statement query = db_.prepare("SELECT " + kAthleteColumns + " FROM athletes a "
                                  "WHERE a.active = 1 OR ?1 = 1 ORDER BY a.name COLLATE NOCASE");
    query.bindInt(1, includeInactive ? 1 : 0);
    std::vector<Athlete> list;
    while (query.step()) list.push_back(readAthlete(query));
    return list;
}

// ---------------------------------------------------------------------------
// Practices and attendance
// ---------------------------------------------------------------------------

std::int64_t TeamStore::addPractice(const std::string& date, const std::string& title, const std::string& notes) {
    requireDate(date);
    db_.prepare("INSERT INTO practices (date, title, notes) VALUES (?1, ?2, ?3)")
        .bindText(1, date).bindText(2, trim(title)).bindText(3, trim(notes)).run();
    return db_.lastInsertId();
}

void TeamStore::deletePractice(std::int64_t id) {
    db_.prepare("DELETE FROM practices WHERE id = ?1").bindInt(1, id).run();
}

std::optional<Practice> TeamStore::practice(std::int64_t id) {
    Statement query = db_.prepare("SELECT id, date, title, notes FROM practices WHERE id = ?1");
    query.bindInt(1, id);
    if (!query.step()) return std::nullopt;
    return Practice{query.columnInt(0), query.columnText(1), query.columnText(2), query.columnText(3)};
}

std::vector<Practice> TeamStore::practices() {
    // Dates are stored as YYYY-MM-DD, so sorting them as text sorts them by date.
    Statement query = db_.prepare("SELECT id, date, title, notes FROM practices ORDER BY date DESC, id DESC");
    std::vector<Practice> list;
    while (query.step()) {
        list.push_back(Practice{query.columnInt(0), query.columnText(1), query.columnText(2), query.columnText(3)});
    }
    return list;
}

std::vector<SheetRow> TeamStore::attendanceSheet(std::int64_t practiceId) {
    // LEFT JOIN keeps every athlete even when they have no mark for this
    // practice; in that case m.status comes back as NULL.
    Statement query = db_.prepare(
        "SELECT " + kAthleteColumns + ", m.status "
        "FROM athletes a "
        "LEFT JOIN attendance m ON m.athlete_id = a.id AND m.practice_id = ?1 "
        "WHERE a.active = 1 OR m.status IS NOT NULL "
        "ORDER BY a.name COLLATE NOCASE");
    query.bindInt(1, practiceId);
    std::vector<SheetRow> sheet;
    while (query.step()) {
        SheetRow row;
        row.athlete = readAthlete(query);
        if (!query.columnIsNull(8)) row.status = statusFromCode(query.columnText(8));
        sheet.push_back(row);
    }
    return sheet;
}

void TeamStore::setMark(std::int64_t practiceId, std::int64_t athleteId, std::optional<Status> status) {
    if (!status) {
        db_.prepare("DELETE FROM attendance WHERE practice_id = ?1 AND athlete_id = ?2")
            .bindInt(1, practiceId).bindInt(2, athleteId).run();
        return;
    }
    db_.prepare("INSERT INTO attendance (practice_id, athlete_id, status) VALUES (?1, ?2, ?3) "
                "ON CONFLICT (practice_id, athlete_id) DO UPDATE SET status = excluded.status")
        .bindInt(1, practiceId).bindInt(2, athleteId).bindText(3, code(*status)).run();
}

int TeamStore::markRest(std::int64_t practiceId, Status status) {
    if (!practice(practiceId)) return 0;
    // INSERT ... SELECT adds one row for each athlete the SELECT finds:
    // every active athlete who does not already have a mark at this practice.
    db_.prepare(
        "INSERT INTO attendance (practice_id, athlete_id, status) "
        "SELECT ?1, a.id, ?2 FROM athletes a "
        "WHERE a.active = 1 AND NOT EXISTS ("
        "  SELECT 1 FROM attendance m WHERE m.practice_id = ?1 AND m.athlete_id = a.id)")
        .bindInt(1, practiceId).bindText(2, code(status)).run();
    return db_.changes();
}

PracticeSummary TeamStore::practiceSummary(std::int64_t practiceId) {
    PracticeSummary summary;
    for (const SheetRow& row : attendanceSheet(practiceId)) {
        summary.total += 1;
        if (!row.status) continue;
        switch (*row.status) {
            case Status::Present: summary.present += 1; break;
            case Status::Late: summary.late += 1; break;
            case Status::Absent: summary.absent += 1; break;
            case Status::Excused: summary.excused += 1; break;
        }
    }
    return summary;
}

AttendanceSummary TeamStore::attendanceSummary(std::int64_t athleteId) {
    // GROUP BY collapses the athlete's marks into one row per status with a count.
    Statement query = db_.prepare("SELECT status, COUNT(*) FROM attendance WHERE athlete_id = ?1 GROUP BY status");
    query.bindInt(1, athleteId);
    AttendanceSummary summary;
    while (query.step()) {
        const int count = static_cast<int>(query.columnInt(1));
        switch (statusFromCode(query.columnText(0)).value_or(Status::Present)) {
            case Status::Present: summary.present = count; break;
            case Status::Late: summary.late = count; break;
            case Status::Absent: summary.absent = count; break;
            case Status::Excused: summary.excused = count; break;
        }
    }
    return summary;
}

std::vector<RecentMark> TeamStore::recentMarks(std::int64_t athleteId, int count) {
    // The inner SELECT picks the latest practices; the outer one attaches this
    // athlete's mark to each and puts them back in date order.
    Statement query = db_.prepare(
        "SELECT p.date, m.status "
        "FROM (SELECT id, date FROM practices ORDER BY date DESC, id DESC LIMIT ?2) p "
        "LEFT JOIN attendance m ON m.practice_id = p.id AND m.athlete_id = ?1 "
        "ORDER BY p.date ASC, p.id ASC");
    query.bindInt(1, athleteId).bindInt(2, count);
    std::vector<RecentMark> marks;
    while (query.step()) {
        RecentMark mark;
        mark.date = query.columnText(0);
        if (!query.columnIsNull(1)) mark.status = statusFromCode(query.columnText(1));
        marks.push_back(mark);
    }
    return marks;
}

// ---------------------------------------------------------------------------
// Tests and results
// ---------------------------------------------------------------------------

std::int64_t TeamStore::addTest(const std::string& name, TestKind kind, const std::string& unit, bool lowerIsBetter) {
    const std::string cleaned = trim(name);
    if (cleaned.empty()) throw ValidationError("Enter a name for the test.");
    const bool isTime = kind == TestKind::Time;
    db_.prepare("INSERT INTO tests (name, kind, unit, lower_is_better) VALUES (?1, ?2, ?3, ?4)")
        .bindText(1, cleaned)
        .bindText(2, isTime ? "time" : "number")
        .bindText(3, isTime ? "" : trim(unit))          // times have no unit
        .bindInt(4, isTime || lowerIsBetter ? 1 : 0)    // a faster time is always better
        .run();
    return db_.lastInsertId();
}

void TeamStore::deleteTest(std::int64_t id) {
    db_.prepare("DELETE FROM tests WHERE id = ?1").bindInt(1, id).run();
}

std::optional<Test> TeamStore::test(std::int64_t id) {
    Statement query = db_.prepare("SELECT id, name, kind, unit, lower_is_better FROM tests WHERE id = ?1");
    query.bindInt(1, id);
    if (!query.step()) return std::nullopt;
    return readTest(query);
}

std::vector<Test> TeamStore::tests() {
    Statement query = db_.prepare("SELECT id, name, kind, unit, lower_is_better FROM tests ORDER BY id");
    std::vector<Test> list;
    while (query.step()) list.push_back(readTest(query));
    return list;
}

std::int64_t TeamStore::addResult(std::int64_t athleteId, std::int64_t testId, const std::string& date, double value) {
    requireDate(date);
    if (!(value >= 0)) throw ValidationError("A result cannot be negative.");
    if (!athlete(athleteId)) throw ValidationError("That athlete no longer exists.");
    if (!test(testId)) throw ValidationError("That test no longer exists.");
    db_.prepare("INSERT INTO results (athlete_id, test_id, date, value) VALUES (?1, ?2, ?3, ?4)")
        .bindInt(1, athleteId).bindInt(2, testId).bindText(3, date).bindDouble(4, value).run();
    return db_.lastInsertId();
}

void TeamStore::deleteResult(std::int64_t id) {
    db_.prepare("DELETE FROM results WHERE id = ?1").bindInt(1, id).run();
}

std::vector<Result> TeamStore::results(std::int64_t athleteId, std::int64_t testId) {
    Statement query = db_.prepare("SELECT id, athlete_id, test_id, date, value FROM results "
                                  "WHERE athlete_id = ?1 AND test_id = ?2 ORDER BY date ASC, id ASC");
    query.bindInt(1, athleteId).bindInt(2, testId);
    std::vector<Result> list;
    while (query.step()) {
        list.push_back(Result{query.columnInt(0), query.columnInt(1), query.columnInt(2), query.columnText(3), query.columnDouble(4)});
    }
    return list;
}

std::vector<RankingRow> TeamStore::ranking(std::int64_t testId) {
    const std::optional<Test> test = this->test(testId);
    if (!test) return {};

    // "Best" is the smallest value for a time trial and the largest for
    // something like pull-ups, so the SQL is built for whichever applies.
    // Only these two fixed pieces of text vary; every value still goes
    // through a ?1 placeholder.
    const std::string best = test->lowerIsBetter ? "MIN(r.value)" : "MAX(r.value)";
    const std::string direction = test->lowerIsBetter ? "ASC" : "DESC";

    // GROUP BY a.id produces one row per athlete. The two small sub-queries
    // fetch that athlete's most recent result and its date.
    Statement query = db_.prepare(
        "SELECT " + kAthleteColumns + ", " + best + " AS best, COUNT(*) AS result_count, "
        "  (SELECT r2.value FROM results r2 WHERE r2.athlete_id = a.id AND r2.test_id = ?1 "
        "   ORDER BY r2.date DESC, r2.id DESC LIMIT 1) AS latest, "
        "  (SELECT r2.date FROM results r2 WHERE r2.athlete_id = a.id AND r2.test_id = ?1 "
        "   ORDER BY r2.date DESC, r2.id DESC LIMIT 1) AS latest_date "
        "FROM results r JOIN athletes a ON a.id = r.athlete_id "
        "WHERE r.test_id = ?1 AND a.active = 1 "
        "GROUP BY a.id "
        "ORDER BY best " + direction + ", a.name COLLATE NOCASE");
    query.bindInt(1, testId);

    std::vector<RankingRow> rows;
    while (query.step()) {
        RankingRow row;
        row.athlete = readAthlete(query);
        row.best = query.columnDouble(8);
        row.count = static_cast<int>(query.columnInt(9));
        row.latest = query.columnDouble(10);
        row.latestDate = query.columnText(11);

        // Athletes with exactly the same best result share a rank, and the
        // next athlete's rank skips ahead (1, 2, 2, 4).
        const int position = static_cast<int>(rows.size()) + 1;
        row.rank = (!rows.empty() && rows.back().best == row.best) ? rows.back().rank : position;
        const double leader = rows.empty() ? row.best : rows.front().best;
        row.gap = row.best > leader ? row.best - leader : leader - row.best;
        rows.push_back(row);
    }
    return rows;
}

// ---------------------------------------------------------------------------
// Whole-team operations
// ---------------------------------------------------------------------------

Counts TeamStore::counts() {
    Statement query = db_.prepare("SELECT (SELECT COUNT(*) FROM athletes), (SELECT COUNT(*) FROM practices), "
                                  "(SELECT COUNT(*) FROM results)");
    query.step();
    return Counts{static_cast<int>(query.columnInt(0)), static_cast<int>(query.columnInt(1)),
                  static_cast<int>(query.columnInt(2))};
}

void TeamStore::eraseAll() {
    inTransaction([this] {
        // Deleting athletes, practices and tests also clears attendance and
        // results, because of the ON DELETE CASCADE rules.
        db_.execute("DELETE FROM athletes; DELETE FROM practices; DELETE FROM tests; DELETE FROM settings;");
        addDefaultTests();
    });
}

std::vector<AttendanceRecord> TeamStore::allAttendance() {
    Statement query = db_.prepare(
        "SELECT p.date, p.title, a.name, m.status "
        "FROM attendance m "
        "JOIN practices p ON p.id = m.practice_id "
        "JOIN athletes a ON a.id = m.athlete_id "
        "ORDER BY p.date, p.id, a.name COLLATE NOCASE");
    std::vector<AttendanceRecord> records;
    while (query.step()) {
        records.push_back(AttendanceRecord{query.columnText(0), query.columnText(1), query.columnText(2),
                                           statusFromCode(query.columnText(3)).value_or(Status::Present)});
    }
    return records;
}

std::vector<ResultRecord> TeamStore::allResults() {
    Statement query = db_.prepare(
        "SELECT r.date, t.id, t.name, t.kind, t.unit, t.lower_is_better, a.name, r.value "
        "FROM results r "
        "JOIN tests t ON t.id = r.test_id "
        "JOIN athletes a ON a.id = r.athlete_id "
        "ORDER BY r.date, t.id, a.name COLLATE NOCASE");
    std::vector<ResultRecord> records;
    while (query.step()) {
        ResultRecord record;
        record.date = query.columnText(0);
        record.test = readTest(query, 1);
        record.athleteName = query.columnText(6);
        record.value = query.columnDouble(7);
        records.push_back(record);
    }
    return records;
}

void TeamStore::inTransaction(const std::function<void()>& work) {
    Transaction transaction(db_);
    work();
    transaction.commit();
}

}  // namespace ptt
