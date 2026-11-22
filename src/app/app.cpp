#include "app.h"

#include "../core/csv.h"
#include "../core/format.h"
#include "../core/sample_data.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace ptt {

namespace {

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::vector<std::string> words(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> list;
    for (std::string word; stream >> word;) list.push_back(word);
    return list;
}

std::string statusText(const std::optional<Status>& status) {
    return status ? label(*status) : "-";
}

std::string practiceName(const Practice& practice) {
    return practice.title.empty() ? practice.date : practice.date + "  " + practice.title;
}

}  // namespace

App::App(TeamStore& store, std::istream& in, std::ostream& out) : store_(store), io_(in, out) {}

void App::run() {
    while (true) {
        io_.heading(store_.teamName() + " - Paddle Team Tracker");
        switch (io_.menu({"Roster", "Practices and attendance", "Performance", "Team data"}, "Quit")) {
            case 1: rosterMenu(); break;
            case 2: practicesMenu(); break;
            case 3: performanceMenu(); break;
            case 4: dataMenu(); break;
            default: return;
        }
    }
}

void App::attempt(const std::function<void()>& action) {
    try {
        action();
    } catch (const ValidationError& error) {
        io_.line(std::string("Not saved: ") + error.what());
    }
}

// ---------------------------------------------------------------------------
// Roster
// ---------------------------------------------------------------------------

void App::rosterMenu() {
    bool showInactive = false;
    while (true) {
        io_.heading("Roster");
        printRoster(showInactive);
        const int choice = io_.menu({"Add athlete", "View athlete", "Edit athlete", "Deactivate or reactivate athlete",
                                     "Delete athlete", showInactive ? "Hide inactive athletes" : "Show inactive athletes"});
        switch (choice) {
            case 1: attempt([this] { addAthlete(); }); break;
            case 2: viewAthlete(); break;
            case 3: attempt([this] { editAthlete(); }); break;
            case 4: toggleActive(); break;
            case 5: deleteAthlete(); break;
            case 6: showInactive = !showInactive; break;
            default: return;
        }
    }
}

void App::printRoster(bool includeInactive) {
    const std::vector<Athlete> athletes = store_.athletes(includeInactive);
    if (athletes.empty()) {
        io_.line("No athletes yet. Add one here, or load the sample crew from Team data.");
        return;
    }
    const std::string unit = store_.weightUnit();
    int left = 0, right = 0, both = 0, active = 0;
    std::vector<std::vector<std::string>> rows;
    for (std::size_t i = 0; i < athletes.size(); ++i) {
        const Athlete& a = athletes[i];
        if (a.active) {
            active += 1;
            if (a.side == Side::Left) left += 1; else if (a.side == Side::Right) right += 1; else both += 1;
        }
        rows.push_back({std::to_string(i + 1), a.active ? a.name : a.name + " (inactive)", label(a.side), label(a.role),
                        a.weight ? formatNumber(*a.weight) + " " + unit : "",
                        formatPercent(store_.attendanceSummary(a.id).rate())});
    }
    io_.line(std::to_string(active) + " active: " + std::to_string(left) + " left, " + std::to_string(right) +
             " right, " + std::to_string(both) + " both");
    io_.line();
    io_.table({"#", "Name", "Side", "Role", "Weight", "Attendance"}, rows, {0, 4, 5});
}

std::optional<Athlete> App::pickAthlete(const std::string& prompt, bool includeInactive) {
    const std::vector<Athlete> athletes = store_.athletes(includeInactive);
    if (athletes.empty()) {
        io_.line("There are no athletes to choose from.");
        return std::nullopt;
    }
    io_.line();
    for (std::size_t i = 0; i < athletes.size(); ++i) {
        io_.line("  " + std::to_string(i + 1) + ". " + athletes[i].name + (athletes[i].active ? "" : " (inactive)"));
    }
    const std::optional<int> choice = io_.askNumberOrCancel(prompt + " (number, or Enter to cancel):", 1,
                                                            static_cast<int>(athletes.size()));
    if (!choice) return std::nullopt;
    return athletes[static_cast<std::size_t>(*choice - 1)];
}

Athlete App::askAthleteDetails(Athlete athlete) {
    // Each question shows the current value in brackets; Enter keeps it.
    const auto field = [this](const std::string& prompt, const std::string& current) {
        return current.empty() ? io_.ask(prompt + ":") : io_.askKeeping(prompt, current);
    };

    athlete.name = field("Name", athlete.name);

    const std::optional<Side> side = sideFromCode(upper(field("Side (L, R or B for both)", code(athlete.side))));
    if (!side) throw ValidationError("Side must be L, R or B.");
    athlete.side = *side;

    const std::string roleAnswer = upper(field("Role (P paddler, D drummer, S steer)", upper(code(athlete.role)).substr(0, 1)));
    if (roleAnswer == "P" || roleAnswer == "PADDLER") athlete.role = Role::Paddler;
    else if (roleAnswer == "D" || roleAnswer == "DRUMMER") athlete.role = Role::Drummer;
    else if (roleAnswer == "S" || roleAnswer == "STEER") athlete.role = Role::Steer;
    else throw ValidationError("Role must be P, D or S.");

    const std::string gender = upper(field("Gender (F, M, or - for not set)", athlete.gender.empty() ? "-" : athlete.gender));
    athlete.gender = gender == "-" ? "" : gender;

    const std::string weightPrompt = "Weight in " + store_.weightUnit() + " (or - for not set)";
    const std::string weight = field(weightPrompt, athlete.weight ? formatNumber(*athlete.weight) : "-");
    if (weight == "-") {
        athlete.weight.reset();
    } else {
        athlete.weight = parseNumber(weight);
        if (!athlete.weight) throw ValidationError("Weight must be a number.");
    }

    const std::string notes = field("Notes (or - for none)", athlete.notes.empty() ? "-" : athlete.notes);
    athlete.notes = notes == "-" ? "" : notes;
    return athlete;
}

void App::addAthlete() {
    io_.heading("Add athlete");
    const Athlete athlete = askAthleteDetails(Athlete{});
    store_.addAthlete(athlete);
    io_.line("Added " + trim(athlete.name) + ".");
}

void App::editAthlete() {
    const std::optional<Athlete> athlete = pickAthlete("Edit which athlete?", true);
    if (!athlete) return;
    io_.heading("Edit " + athlete->name);
    io_.line("Press Enter to keep the value shown in brackets.");
    store_.updateAthlete(askAthleteDetails(*athlete));
    io_.line("Saved.");
}

void App::toggleActive() {
    std::optional<Athlete> athlete = pickAthlete("Deactivate or reactivate which athlete?", true);
    if (!athlete) return;
    athlete->active = !athlete->active;
    store_.updateAthlete(*athlete);
    io_.line(athlete->name + (athlete->active ? " is active again." : " is now inactive. Their history is kept."));
}

void App::deleteAthlete() {
    const std::optional<Athlete> athlete = pickAthlete("Delete which athlete?", true);
    if (!athlete) return;
    io_.line("Deleting " + athlete->name + " also deletes their attendance marks and results. This cannot be undone.");
    io_.line("To keep their history, deactivate them instead.");
    if (!io_.confirm("Delete " + athlete->name + "?")) return;
    store_.deleteAthlete(athlete->id);
    io_.line("Deleted.");
}

void App::viewAthlete() {
    const std::optional<Athlete> picked = pickAthlete("View which athlete?", true);
    if (!picked) return;
    const Athlete& a = *picked;

    io_.heading(a.name);
    std::string facts = std::string(label(a.side)) + " side, " + label(a.role);
    if (!a.gender.empty()) facts += ", " + a.gender;
    if (a.weight) facts += ", " + formatNumber(*a.weight) + " " + store_.weightUnit();
    if (!a.active) facts += ", inactive";
    io_.line(facts);
    if (!a.notes.empty()) io_.line("Notes: " + a.notes);

    const AttendanceSummary summary = store_.attendanceSummary(a.id);
    io_.line();
    io_.line("Attendance: " + formatPercent(summary.rate()) + " (" + std::to_string(summary.present) + " present, " +
             std::to_string(summary.late) + " late, " + std::to_string(summary.absent) + " absent, " +
             std::to_string(summary.excused) + " excused; excused practices are not counted)");
    std::string recent;
    for (const RecentMark& mark : store_.recentMarks(a.id)) {
        recent += mark.status ? code(*mark.status) : "-";
        recent += ' ';
    }
    if (!recent.empty()) io_.line("Last practices, oldest first: " + recent);

    bool anyResults = false;
    for (const Test& test : store_.tests()) {
        const std::vector<Result> results = store_.results(a.id, test.id);
        if (results.empty()) continue;
        anyResults = true;
        // The best result is the smallest or the largest, depending on the test.
        const auto best = std::min_element(results.begin(), results.end(), [&test](const Result& x, const Result& y) {
            return test.lowerIsBetter ? x.value < y.value : x.value > y.value;
        });
        io_.line();
        io_.line(test.name + " (best " + formatResult(test, best->value) + ")");
        for (const Result& result : results) {
            io_.line("  " + result.date + "  " + formatResult(test, result.value) + (result.id == best->id ? "  best" : ""));
        }
    }
    if (!anyResults) {
        io_.line();
        io_.line("No results logged yet.");
    }
    io_.line();
    io_.pause();
}

// ---------------------------------------------------------------------------
// Practices and attendance
// ---------------------------------------------------------------------------

std::string App::summaryText(const PracticeSummary& s) {
    if (s.total == 0) return "No athletes on the roster";
    if (s.marked() == 0) return "Attendance not taken";
    std::string text = std::to_string(s.onWater()) + " of " + std::to_string(s.total) + " on the water";
    if (s.late > 0) text += ", " + std::to_string(s.late) + " late";
    if (s.absent > 0) text += ", " + std::to_string(s.absent) + " absent";
    if (s.excused > 0) text += ", " + std::to_string(s.excused) + " excused";
    return text;
}

void App::practicesMenu() {
    while (true) {
        io_.heading("Practices");
        printPractices();
        switch (io_.menu({"New practice", "Take or change attendance", "Delete practice"})) {
            case 1: attempt([this] { newPractice(); }); break;
            case 2:
                if (const std::optional<Practice> practice = pickPractice("Attendance for which practice?")) {
                    takeAttendance(practice->id);
                }
                break;
            case 3: deletePractice(); break;
            default: return;
        }
    }
}

void App::printPractices() {
    const std::vector<Practice> practices = store_.practices();
    if (practices.empty()) {
        io_.line("No practices yet. Create one, then mark who showed up.");
        return;
    }
    std::vector<std::vector<std::string>> rows;
    for (std::size_t i = 0; i < practices.size(); ++i) {
        rows.push_back({std::to_string(i + 1), practices[i].date, practices[i].title,
                        summaryText(store_.practiceSummary(practices[i].id))});
    }
    io_.table({"#", "Date", "Name", "Attendance"}, rows, {0});
}

std::optional<Practice> App::pickPractice(const std::string& prompt) {
    const std::vector<Practice> practices = store_.practices();
    if (practices.empty()) {
        io_.line("There are no practices to choose from.");
        return std::nullopt;
    }
    const std::optional<int> choice = io_.askNumberOrCancel(prompt + " (number from the list, or Enter to cancel):", 1,
                                                            static_cast<int>(practices.size()));
    if (!choice) return std::nullopt;
    return practices[static_cast<std::size_t>(*choice - 1)];
}

std::string App::askDate(const std::string& prompt) {
    while (true) {
        const std::string date = io_.askKeeping(prompt + " (YYYY-MM-DD)", todayIso());
        if (isValidDate(date)) return date;
        io_.line("Enter the date as YYYY-MM-DD, for example 2026-03-14.");
    }
}

void App::newPractice() {
    io_.heading("New practice");
    const std::string date = askDate("Date");
    const std::string title = io_.ask("Name, such as \"Race pieces\" (optional):");
    const std::int64_t id = store_.addPractice(date, title);
    takeAttendance(id);
}

void App::takeAttendance(std::int64_t practiceId) {
    const std::optional<Practice> practice = store_.practice(practiceId);
    if (!practice) return;

    while (true) {
        const std::vector<SheetRow> sheet = store_.attendanceSheet(practiceId);
        io_.heading("Attendance: " + practiceName(*practice));
        if (sheet.empty()) {
            io_.line("No athletes on the roster. Add athletes first.");
            return;
        }
        std::vector<std::vector<std::string>> rows;
        for (std::size_t i = 0; i < sheet.size(); ++i) {
            rows.push_back({std::to_string(i + 1), sheet[i].athlete.name, label(sheet[i].athlete.side), statusText(sheet[i].status)});
        }
        io_.table({"#", "Athlete", "Side", "Status"}, rows, {0});
        io_.line();
        io_.line(summaryText(store_.practiceSummary(practiceId)));
        io_.line();
        io_.line("Type athlete numbers, then a status: P present, L late, A absent, E excused, - to clear.");
        io_.line("  3 A      marks athlete 3 absent");
        io_.line("  2 5 L    marks athletes 2 and 5 late");
        io_.line("  rest P   marks everyone not yet marked as present");

        const std::vector<std::string> parts = words(io_.ask("Mark (Enter to finish):"));
        if (parts.empty()) return;

        // The last word is the status; everything before it says who it applies to.
        const std::string statusWord = upper(parts.back());
        const std::optional<Status> status = statusFromCode(statusWord);
        if (!status && statusWord != "-") {
            io_.line("End with a status: P, L, A, E, or - to clear.");
            continue;
        }
        if (parts.size() == 1) {
            io_.line("Put athlete numbers (or \"rest\") before the status.");
            continue;
        }

        if (parts.size() == 2 && upper(parts[0]) == "REST") {
            if (!status) {
                io_.line("\"rest\" needs a status to set: P, L, A or E.");
                continue;
            }
            io_.line("Marked " + std::to_string(store_.markRest(practiceId, *status)) + ".");
            continue;
        }

        // Check every number before changing anything, so a typo changes nothing.
        std::vector<std::int64_t> athleteIds;
        bool valid = true;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
            const std::optional<double> number = parseNumber(parts[i]);
            const bool whole = number && *number == static_cast<int>(*number);
            if (!whole || *number < 1 || *number > static_cast<double>(sheet.size())) {
                io_.line("\"" + parts[i] + "\" is not a number from 1 to " + std::to_string(sheet.size()) + ".");
                valid = false;
                break;
            }
            athleteIds.push_back(sheet[static_cast<std::size_t>(*number) - 1].athlete.id);
        }
        if (!valid) continue;
        store_.inTransaction([&] {
            for (const std::int64_t athleteId : athleteIds) store_.setMark(practiceId, athleteId, status);
        });
    }
}

void App::deletePractice() {
    const std::optional<Practice> practice = pickPractice("Delete which practice?");
    if (!practice) return;
    io_.line("Deleting " + practiceName(*practice) + " also deletes its attendance marks. This cannot be undone.");
    if (!io_.confirm("Delete this practice?")) return;
    store_.deletePractice(practice->id);
    io_.line("Deleted.");
}

// ---------------------------------------------------------------------------
// Performance
// ---------------------------------------------------------------------------

void App::performanceMenu() {
    while (true) {
        // Use the test chosen last time, or the first one if that is gone.
        std::optional<Test> test = currentTestId_ ? store_.test(*currentTestId_) : std::nullopt;
        if (!test) {
            const std::vector<Test> tests = store_.tests();
            if (!tests.empty()) test = tests.front();
        }
        if (!test) {
            io_.heading("Performance");
            io_.line("No tests set up. Add a test such as a 500 m time trial, then log results for it.");
            if (io_.menu({"Manage tests"}) == 0) return;
            manageTests();
            continue;
        }
        currentTestId_ = test->id;

        io_.heading("Performance: " + test->name);
        printRanking(*test);
        switch (io_.menu({"Log results for a session", "Add one result", "Delete a result", "Change test", "Manage tests"})) {
            case 1: attempt([&] { logSession(*test); }); break;
            case 2: attempt([&] { addOneResult(*test); }); break;
            case 3: deleteResult(*test); break;
            case 4:
                if (const std::optional<Test> chosen = pickTest("Show which test?")) currentTestId_ = chosen->id;
                break;
            case 5: manageTests(); break;
            default: return;
        }
    }
}

void App::printRanking(const Test& test) {
    const std::vector<RankingRow> ranking = store_.ranking(test.id);
    if (ranking.empty()) {
        io_.line("No results for this test yet. Log a session to rank the crew.");
        return;
    }
    std::vector<std::vector<std::string>> rows;
    for (const RankingRow& row : ranking) {
        std::string gap;
        if (row.gap > 0) {
            gap = (test.lowerIsBetter ? "+" : "-") + (test.kind == TestKind::Time ? formatTime(row.gap) : formatNumber(row.gap));
        }
        rows.push_back({std::to_string(row.rank), row.athlete.name, label(row.athlete.side), formatResult(test, row.best), gap,
                        formatResult(test, row.latest) + " (" + row.latestDate + ")", std::to_string(row.count)});
    }
    io_.table({"Rank", "Athlete", "Side", "Best", "Gap", "Latest", "Results"}, rows, {0, 3, 4, 5, 6});
    io_.line();
    io_.line(std::string("Ranked by each active athlete's best result. ") +
             (test.lowerIsBetter ? "Lower is better." : "Higher is better."));
}

std::optional<Test> App::pickTest(const std::string& prompt) {
    const std::vector<Test> tests = store_.tests();
    if (tests.empty()) {
        io_.line("There are no tests to choose from.");
        return std::nullopt;
    }
    io_.line();
    for (std::size_t i = 0; i < tests.size(); ++i) {
        io_.line("  " + std::to_string(i + 1) + ". " + tests[i].name);
    }
    const std::optional<int> choice = io_.askNumberOrCancel(prompt + " (number, or Enter to cancel):", 1,
                                                            static_cast<int>(tests.size()));
    if (!choice) return std::nullopt;
    return tests[static_cast<std::size_t>(*choice - 1)];
}

void App::logSession(const Test& test) {
    const std::vector<Athlete> athletes = store_.athletes();
    if (athletes.empty()) {
        io_.line("No athletes on the roster. Add athletes first.");
        return;
    }
    io_.heading("Log results: " + test.name);
    const std::string date = askDate("Date");
    const std::string hint = test.kind == TestKind::Time ? "Times as m:ss.xx (2:05.30) or seconds (58.4)." : "Plain numbers.";
    io_.line(hint + " Press Enter to skip an athlete.");

    std::vector<std::pair<std::int64_t, double>> entries;
    for (const Athlete& athlete : athletes) {
        while (true) {
            const std::string answer = io_.ask(athlete.name + ":");
            if (answer.empty()) break;
            if (const std::optional<double> value = parseResult(test, answer)) {
                entries.emplace_back(athlete.id, *value);
                break;
            }
            io_.line("That is not a valid result. " + hint);
        }
    }

    // Save the whole session at once: either every result is stored or none.
    store_.inTransaction([&] {
        for (const auto& [athleteId, value] : entries) store_.addResult(athleteId, test.id, date, value);
    });
    io_.line("Saved " + std::to_string(entries.size()) + (entries.size() == 1 ? " result." : " results."));
}

void App::addOneResult(const Test& test) {
    const std::optional<Athlete> athlete = pickAthlete("Add a result for which athlete?", false);
    if (!athlete) return;
    const std::string date = askDate("Date");
    const std::optional<double> value = parseResult(test, io_.ask(test.name + " result for " + athlete->name + ":"));
    if (!value) {
        throw ValidationError(test.kind == TestKind::Time ? "Enter a time like 2:05.30 or 58.4." : "Enter a number.");
    }
    store_.addResult(athlete->id, test.id, date, *value);
    io_.line("Saved " + formatResult(test, *value) + ".");
}

void App::deleteResult(const Test& test) {
    const std::optional<Athlete> athlete = pickAthlete("Delete a result for which athlete?", true);
    if (!athlete) return;
    const std::vector<Result> results = store_.results(athlete->id, test.id);
    if (results.empty()) {
        io_.line(athlete->name + " has no results for this test.");
        return;
    }
    io_.line();
    for (std::size_t i = 0; i < results.size(); ++i) {
        io_.line("  " + std::to_string(i + 1) + ". " + results[i].date + "  " + formatResult(test, results[i].value));
    }
    const std::optional<int> choice = io_.askNumberOrCancel("Delete which result? (number, or Enter to cancel):", 1,
                                                            static_cast<int>(results.size()));
    if (!choice) return;
    store_.deleteResult(results[static_cast<std::size_t>(*choice - 1)].id);
    io_.line("Deleted.");
}

void App::manageTests() {
    while (true) {
        io_.heading("Tests");
        std::vector<std::vector<std::string>> rows;
        const std::vector<Test> tests = store_.tests();
        for (std::size_t i = 0; i < tests.size(); ++i) {
            const Test& t = tests[i];
            const std::string type = t.kind == TestKind::Time ? "Time"
                : std::string("Number") + (t.unit.empty() ? "" : " in " + t.unit) + (t.lowerIsBetter ? ", lower is better" : ", higher is better");
            rows.push_back({std::to_string(i + 1), t.name, type});
        }
        if (rows.empty()) io_.line("No tests yet."); else io_.table({"#", "Test", "Measured as"}, rows, {0});

        const int choice = io_.menu({"Add test", "Delete test"});
        if (choice == 0) return;
        if (choice == 1) {
            attempt([this] {
                const std::string name = io_.ask("Test name, such as \"1 km time trial\" or \"Pull-ups\":");
                io_.line("  1. Time (lower is better)");
                io_.line("  2. Number (higher is better)");
                io_.line("  3. Number (lower is better)");
                const int type = io_.askNumber("Measured as:", 1, 3);
                const std::string unit = type == 1 ? "" : io_.ask("Unit, such as reps, kg or watts (optional):");
                store_.addTest(name, type == 1 ? TestKind::Time : TestKind::Number, unit, type != 2);
                io_.line("Added.");
            });
        } else if (const std::optional<Test> test = pickTest("Delete which test?")) {
            io_.line("Deleting the " + test->name + " also deletes every result logged for it. This cannot be undone.");
            if (io_.confirm("Delete this test?")) {
                store_.deleteTest(test->id);
                io_.line("Deleted.");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Team data
// ---------------------------------------------------------------------------

void App::dataMenu() {
    while (true) {
        const Counts counts = store_.counts();
        io_.heading("Team data");
        io_.line("Team name:   " + store_.teamName());
        io_.line("Weight unit: " + store_.weightUnit());
        io_.line("Saved:       " + std::to_string(counts.athletes) + " athletes, " + std::to_string(counts.practices) +
                 " practices, " + std::to_string(counts.results) + " results");

        switch (io_.menu({"Rename team", "Switch weight unit (lb or kg)", "Export CSV files", "Load sample crew", "Erase everything"})) {
            case 1:
                attempt([this] { store_.setTeamName(io_.askKeeping("Team name", store_.teamName())); });
                break;
            case 2:
                store_.setWeightUnit(store_.weightUnit() == "lb" ? "kg" : "lb");
                io_.line("Weights are now labelled in " + store_.weightUnit() + ". Existing numbers are not converted.");
                break;
            case 3: exportCsvFiles(); break;
            case 4:
                if (counts.athletes + counts.practices + counts.results > 0) {
                    io_.line("Loading the sample crew replaces everything currently saved. This cannot be undone.");
                    if (!io_.confirm("Replace your data with the sample crew?")) break;
                }
                loadSampleCrew(store_, todayIso());
                currentTestId_.reset();
                io_.line("Sample crew loaded.");
                break;
            case 5:
                io_.line("This deletes all athletes, practices, results and tests. It cannot be undone.");
                if (io_.confirm("Erase everything?")) {
                    store_.eraseAll();
                    currentTestId_.reset();
                    io_.line("Everything erased.");
                }
                break;
            default: return;
        }
    }
}

void App::exportCsvFiles() {
    using Exporter = void (*)(TeamStore&, std::ostream&);
    const std::pair<const char*, Exporter> files[] = {
        {"roster.csv", exportRosterCsv}, {"attendance.csv", exportAttendanceCsv}, {"results.csv", exportResultsCsv}};
    for (const auto& [name, exporter] : files) {
        // Binary mode stops Windows from changing the line endings the CSV code writes.
        std::ofstream file(name, std::ios::binary);
        if (!file) {
            io_.line(std::string("Could not write ") + name + ".");
            continue;
        }
        exporter(store_, file);
        io_.line(std::string("Wrote ") + name);
    }
    io_.line("The files are in the folder the program was started from. They open in Excel and Google Sheets.");
}

}  // namespace ptt
