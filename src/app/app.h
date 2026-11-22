// The menus. Each screen prints what it knows, asks what to do, and calls
// TeamStore to do it. No SQL and no calculations live here.
#pragma once

#include "console.h"
#include "../core/team_store.h"

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>

namespace ptt {

class App {
public:
    App(TeamStore& store, std::istream& in, std::ostream& out);

    // Shows the main menu until the person quits.
    void run();

private:
    // Screens
    void rosterMenu();
    void practicesMenu();
    void performanceMenu();
    void dataMenu();

    // Roster
    void printRoster(bool includeInactive);
    void addAthlete();
    void viewAthlete();
    void editAthlete();
    void toggleActive();
    void deleteAthlete();

    // Practices
    void printPractices();
    void newPractice();
    void takeAttendance(std::int64_t practiceId);
    void deletePractice();

    // Performance
    void printRanking(const Test& test);
    void logSession(const Test& test);
    void addOneResult(const Test& test);
    void deleteResult(const Test& test);
    void manageTests();

    // Team data
    void exportCsvFiles();

    // Shared pieces
    std::optional<Athlete> pickAthlete(const std::string& prompt, bool includeInactive);
    std::optional<Practice> pickPractice(const std::string& prompt);
    std::optional<Test> pickTest(const std::string& prompt);
    std::string askDate(const std::string& prompt);
    Athlete askAthleteDetails(Athlete athlete);
    std::string summaryText(const PracticeSummary& summary);
    // Runs one action and, if what was entered is not acceptable, prints why
    // and carries on instead of ending the program.
    void attempt(const std::function<void()>& action);

    TeamStore& store_;
    Console io_;
    std::optional<std::int64_t> currentTestId_;
};

}  // namespace ptt
