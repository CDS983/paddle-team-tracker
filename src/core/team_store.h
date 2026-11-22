// Everything the program knows how to do with team data.
//
// TeamStore is the only place that contains SQL. The console menus call these
// functions and never touch the database directly, which keeps the storage
// details in one file and lets the tests exercise the logic without a screen.
#pragma once

#include "database.h"
#include "models.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ptt {

// Thrown when something a person entered is not acceptable (an empty name, an
// impossible date). The message is written to be shown to them as it is.
class ValidationError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class TeamStore {
public:
    // Creates the tables on first use.
    explicit TeamStore(Database& db);

    // ---- Settings ----
    std::string teamName();
    void setTeamName(const std::string& name);
    std::string weightUnit();                       // "lb" or "kg"
    void setWeightUnit(const std::string& unit);

    // ---- Athletes ----
    std::int64_t addAthlete(const Athlete& athlete);         // returns the new id
    void updateAthlete(const Athlete& athlete);              // matched by athlete.id
    void deleteAthlete(std::int64_t id);                     // also removes their marks and results
    std::optional<Athlete> athlete(std::int64_t id);
    std::vector<Athlete> athletes(bool includeInactive = false);   // sorted by name

    // ---- Practices and attendance ----
    std::int64_t addPractice(const std::string& date, const std::string& title, const std::string& notes = "");
    void deletePractice(std::int64_t id);
    std::optional<Practice> practice(std::int64_t id);
    std::vector<Practice> practices();                       // newest first

    // Active athletes, plus anyone already marked at this practice.
    std::vector<SheetRow> attendanceSheet(std::int64_t practiceId);
    // Pass an empty status to clear a mark.
    void setMark(std::int64_t practiceId, std::int64_t athleteId, std::optional<Status> status);
    // Marks every active athlete who has no mark yet. Returns how many were marked.
    int markRest(std::int64_t practiceId, Status status);
    PracticeSummary practiceSummary(std::int64_t practiceId);

    AttendanceSummary attendanceSummary(std::int64_t athleteId);
    // The athlete's status at the latest `count` practices, oldest first.
    std::vector<RecentMark> recentMarks(std::int64_t athleteId, int count = 10);

    // ---- Tests and results ----
    std::int64_t addTest(const std::string& name, TestKind kind, const std::string& unit = "", bool lowerIsBetter = true);
    void deleteTest(std::int64_t id);                        // also removes its results
    std::optional<Test> test(std::int64_t id);
    std::vector<Test> tests();

    std::int64_t addResult(std::int64_t athleteId, std::int64_t testId, const std::string& date, double value);
    void deleteResult(std::int64_t id);
    // One athlete's results for one test, oldest first.
    std::vector<Result> results(std::int64_t athleteId, std::int64_t testId);
    // Each active athlete's best result for a test, best first.
    std::vector<RankingRow> ranking(std::int64_t testId);

    // ---- Whole-team operations ----
    Counts counts();
    void eraseAll();                                         // back to an empty team with the default tests
    std::vector<AttendanceRecord> allAttendance();           // for the CSV export, oldest first
    std::vector<ResultRecord> allResults();                  // for the CSV export, oldest first
    // Runs `work` so that either all of its changes are saved or none are.
    void inTransaction(const std::function<void()>& work);

private:
    void createSchema();
    void addDefaultTests();
    std::string setting(const std::string& key, const std::string& fallback);
    void setSetting(const std::string& key, const std::string& value);

    Database& db_;
};

}  // namespace ptt
