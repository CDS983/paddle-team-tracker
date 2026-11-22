// The plain data types the rest of the program passes around.
//
// Each enum has a short "code" (what is stored in the database) and a "label"
// (what is shown to the person using the program).
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ptt {

enum class Side { Left, Right, Both };
enum class Role { Paddler, Drummer, Steer };
enum class Status { Present, Late, Absent, Excused };
enum class TestKind { Time, Number };

inline const char* code(Side side) {
    switch (side) {
        case Side::Left: return "L";
        case Side::Right: return "R";
        case Side::Both: return "B";
    }
    return "B";
}

inline const char* label(Side side) {
    switch (side) {
        case Side::Left: return "Left";
        case Side::Right: return "Right";
        case Side::Both: return "Both";
    }
    return "Both";
}

inline std::optional<Side> sideFromCode(const std::string& text) {
    if (text == "L") return Side::Left;
    if (text == "R") return Side::Right;
    if (text == "B") return Side::Both;
    return std::nullopt;
}

inline const char* code(Role role) {
    switch (role) {
        case Role::Paddler: return "paddler";
        case Role::Drummer: return "drummer";
        case Role::Steer: return "steer";
    }
    return "paddler";
}

inline const char* label(Role role) {
    switch (role) {
        case Role::Paddler: return "Paddler";
        case Role::Drummer: return "Drummer";
        case Role::Steer: return "Steer";
    }
    return "Paddler";
}

inline std::optional<Role> roleFromCode(const std::string& text) {
    if (text == "paddler") return Role::Paddler;
    if (text == "drummer") return Role::Drummer;
    if (text == "steer") return Role::Steer;
    return std::nullopt;
}

inline const char* code(Status status) {
    switch (status) {
        case Status::Present: return "P";
        case Status::Late: return "L";
        case Status::Absent: return "A";
        case Status::Excused: return "E";
    }
    return "P";
}

inline const char* label(Status status) {
    switch (status) {
        case Status::Present: return "Present";
        case Status::Late: return "Late";
        case Status::Absent: return "Absent";
        case Status::Excused: return "Excused";
    }
    return "Present";
}

inline std::optional<Status> statusFromCode(const std::string& text) {
    if (text == "P") return Status::Present;
    if (text == "L") return Status::Late;
    if (text == "A") return Status::Absent;
    if (text == "E") return Status::Excused;
    return std::nullopt;
}

struct Athlete {
    std::int64_t id = 0;
    std::string name;
    Side side = Side::Both;
    Role role = Role::Paddler;
    std::string gender;             // "", "F" or "M"
    std::optional<double> weight;   // in the team's weight unit; empty when not recorded
    std::string notes;
    bool active = true;             // inactive athletes keep their history but leave new sheets
};

struct Practice {
    std::int64_t id = 0;
    std::string date;               // "YYYY-MM-DD"
    std::string title;
    std::string notes;
};

// Something the crew is measured on: a timed piece, or a plain number such as pull-ups.
struct Test {
    std::int64_t id = 0;
    std::string name;
    TestKind kind = TestKind::Time;
    std::string unit;               // for Number tests, e.g. "reps"
    bool lowerIsBetter = true;
};

struct Result {
    std::int64_t id = 0;
    std::int64_t athleteId = 0;
    std::int64_t testId = 0;
    std::string date;
    double value = 0;               // seconds for Time tests
};

// One line of a practice's attendance sheet.
struct SheetRow {
    Athlete athlete;
    std::optional<Status> status;   // empty when not marked yet
};

struct AttendanceSummary {
    int present = 0;
    int late = 0;
    int absent = 0;
    int excused = 0;

    // Share of practices attended. Late counts as attended. Excused practices
    // are left out entirely. Empty when there is nothing to count yet.
    std::optional<double> rate() const {
        const int counted = present + late + absent;
        if (counted == 0) return std::nullopt;
        return static_cast<double>(present + late) / counted;
    }
};

struct PracticeSummary {
    int present = 0;
    int late = 0;
    int absent = 0;
    int excused = 0;
    int total = 0;                  // athletes on the sheet

    int marked() const { return present + late + absent + excused; }
    int onWater() const { return present + late; }
};

// An athlete's status at one recent practice.
struct RecentMark {
    std::string date;
    std::optional<Status> status;
};

// One line of the ranking for a test.
struct RankingRow {
    int rank = 0;                   // equal results share a rank
    Athlete athlete;
    double best = 0;
    double gap = 0;                 // distance behind the leader, always >= 0
    double latest = 0;
    std::string latestDate;
    int count = 0;                  // how many results the athlete has for this test
};

struct Counts {
    int athletes = 0;
    int practices = 0;
    int results = 0;
};

// Flat rows used by the CSV exports.
struct AttendanceRecord {
    std::string date;
    std::string practiceTitle;
    std::string athleteName;
    Status status = Status::Present;
};

struct ResultRecord {
    std::string date;
    Test test;
    std::string athleteName;
    double value = 0;
};

}  // namespace ptt
