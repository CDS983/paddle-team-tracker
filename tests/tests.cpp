// Tests for the core logic and a scripted run through the menus.
// Every test uses its own in-memory database, so nothing is written to disk.

#include "test_framework.h"

#include "../src/app/app.h"
#include "../src/core/csv.h"
#include "../src/core/database.h"
#include "../src/core/format.h"
#include "../src/core/sample_data.h"
#include "../src/core/team_store.h"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace ptt;

namespace {

// A database, a store on top of it, and three athletes to work with.
struct Crew {
    Database db{":memory:"};
    TeamStore store{db};
    std::int64_t ana = add("Ana", Side::Left);
    std::int64_t bo = add("Bo", Side::Right);
    std::int64_t cy = add("Cy", Side::Both);

    std::int64_t add(const std::string& name, Side side) {
        Athlete athlete;
        athlete.name = name;
        athlete.side = side;
        return store.addAthlete(athlete);
    }

    // The ids of the three tests every new database starts with.
    std::int64_t test200() { return store.tests().at(0).id; }
    std::int64_t test500() { return store.tests().at(1).id; }
};

// The athletes in a ranking, in order, as one string such as "Bo, Ana".
std::string names(const std::vector<RankingRow>& rows) {
    std::string list;
    for (const RankingRow& row : rows) list += (list.empty() ? "" : ", ") + row.athlete.name;
    return list;
}

int countLines(const std::string& text) {
    int lines = 0;
    for (const char c : text) if (c == '\n') lines += 1;
    return lines;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

// ---- Times, numbers and dates ----

TEST(parse_time_reads_seconds_minutes_and_hours) {
    CHECK_EQ(parseTime("58.4").value_or(-1), 58.4);
    CHECK_EQ(parseTime("2:05.3").value_or(-1), 125.3);
    CHECK_EQ(parseTime("  2:05  ").value_or(-1), 125.0);
    CHECK_EQ(parseTime("1:02:03.5").value_or(-1), 3723.5);
    CHECK_EQ(parseTime("2:05,31").value_or(-1), 125.31);
}

TEST(parse_time_rejects_things_that_are_not_times) {
    for (const char* bad : {"", "abc", "2:75", "1:2:3:4", "-5", "0", "2:05.3s", "2::05", "5.", ".5", "2:5.1.2"}) {
        if (parseTime(bad)) testing::reportFailure(__FILE__, __LINE__, std::string("should be rejected: \"") + bad + "\"");
    }
}

TEST(format_time_writes_hundredths_and_pads_seconds) {
    CHECK_EQ(formatTime(58.4), "58.40");
    CHECK_EQ(formatTime(125.3), "2:05.30");
    CHECK_EQ(formatTime(3723.5), "1:02:03.50");
    CHECK_EQ(formatTime(59.999), "1:00.00");
    CHECK_EQ(formatTime(*parseTime("2:05.31")), "2:05.31");
}

TEST(numbers_parse_and_format) {
    CHECK_EQ(parseNumber("14").value_or(-1), 14.0);
    CHECK_EQ(parseNumber("72,5").value_or(-1), 72.5);
    CHECK(!parseNumber("2:05"));
    CHECK(!parseNumber("-3"));
    CHECK_EQ(formatNumber(14), "14");
    CHECK_EQ(formatNumber(72.5), "72.5");
    CHECK_EQ(formatNumber(100), "100");
    CHECK_EQ(formatNumber(0), "0");
}

TEST(results_format_by_test_kind) {
    Test pullUps;
    pullUps.kind = TestKind::Number;
    pullUps.unit = "reps";
    CHECK_EQ(formatResult(pullUps, 14), "14 reps");
    CHECK_EQ(parseResult(pullUps, "14").value_or(-1), 14.0);
    CHECK(!parseResult(pullUps, "2:05"));
    CHECK_EQ(formatResult(Test{}, 125.3), "2:05.30");
}

TEST(percent_formatting) {
    CHECK_EQ(formatPercent(0.875), "88%");
    CHECK_EQ(formatPercent(1.0), "100%");
    CHECK_EQ(formatPercent(std::nullopt), "-");
}

TEST(dates_are_validated) {
    CHECK(isValidDate("2026-02-28"));
    CHECK(isValidDate("2024-02-29"));
    CHECK(!isValidDate("2026-02-29"));
    CHECK(!isValidDate("2026-13-01"));
    CHECK(!isValidDate("02/28/2026"));
    CHECK(!isValidDate("2026-2-8"));
}

TEST(add_days_crosses_months_and_years) {
    CHECK_EQ(addDays("2026-03-01", -1), "2026-02-28");
    CHECK_EQ(addDays("2026-12-31", 1), "2027-01-01");
    CHECK_EQ(addDays("2026-06-15", -26), "2026-05-20");
    CHECK_EQ(addDays("2026-06-15", 0), "2026-06-15");
}

// ---- Athletes ----

TEST(a_new_database_starts_with_three_tests_and_no_athletes) {
    Database db(":memory:");
    TeamStore store(db);
    CHECK_EQ(store.tests().size(), 3u);
    CHECK_EQ(store.athletes().size(), 0u);
    CHECK_EQ(store.teamName(), "My crew");
    CHECK_EQ(store.weightUnit(), "lb");
}

TEST(saving_an_athlete_requires_a_name_and_tidies_it) {
    Database db(":memory:");
    TeamStore store(db);
    Athlete blank;
    blank.name = "   ";
    CHECK_THROWS(store.addAthlete(blank), ValidationError);

    Athlete ana;
    ana.name = "  Ana  ";
    ana.side = Side::Left;
    ana.weight = 150.5;
    const std::int64_t id = store.addAthlete(ana);
    const Athlete saved = *store.athlete(id);
    CHECK_EQ(saved.name, "Ana");
    CHECK(saved.side == Side::Left);
    CHECK_EQ(saved.weight.value_or(0), 150.5);
    CHECK(saved.active);
}

TEST(invalid_athlete_details_are_rejected) {
    Crew crew;
    Athlete athlete = *crew.store.athlete(crew.ana);
    athlete.gender = "X";
    CHECK_THROWS(crew.store.updateAthlete(athlete), ValidationError);
    athlete.gender = "F";
    athlete.weight = -5;
    CHECK_THROWS(crew.store.updateAthlete(athlete), ValidationError);
}

TEST(editing_an_athlete_updates_them_in_place) {
    Crew crew;
    Athlete ana = *crew.store.athlete(crew.ana);
    ana.name = "Ana Maria";
    ana.active = false;
    crew.store.updateAthlete(ana);
    CHECK_EQ(crew.store.athletes(true).size(), 3u);
    CHECK_EQ(crew.store.athletes().size(), 2u);
    CHECK_EQ(crew.store.athlete(crew.ana)->name, "Ana Maria");
}

TEST(athletes_are_sorted_by_name_ignoring_case) {
    Crew crew;
    crew.add("bea", Side::Left);
    const std::vector<Athlete> list = crew.store.athletes();
    CHECK_EQ(list.at(0).name, "Ana");
    CHECK_EQ(list.at(1).name, "bea");
    CHECK_EQ(list.at(2).name, "Bo");
}

TEST(text_with_quotes_is_stored_safely) {
    Crew crew;
    const std::int64_t id = crew.add("Robert'); DROP TABLE athletes;--", Side::Left);
    CHECK_EQ(crew.store.athlete(id)->name, "Robert'); DROP TABLE athletes;--");
    CHECK_EQ(crew.store.athletes().size(), 4u);
}

// ---- Attendance ----

TEST(attendance_rate_counts_late_as_attended_and_ignores_excused) {
    Crew crew;
    const Status statuses[] = {Status::Present, Status::Late, Status::Absent, Status::Excused, Status::Present};
    int day = 1;
    for (const Status status : statuses) {
        const std::int64_t practice = crew.store.addPractice("2026-03-0" + std::to_string(day++), "");
        crew.store.setMark(practice, crew.ana, status);
    }
    const AttendanceSummary summary = crew.store.attendanceSummary(crew.ana);
    CHECK_EQ(summary.present, 2);
    CHECK_EQ(summary.late, 1);
    CHECK_EQ(summary.absent, 1);
    CHECK_EQ(summary.excused, 1);
    CHECK_EQ(summary.rate().value_or(-1), 0.75);
}

TEST(attendance_rate_is_empty_when_nothing_counts_yet) {
    Crew crew;
    CHECK(!crew.store.attendanceSummary(crew.ana).rate());
    const std::int64_t practice = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(practice, crew.ana, Status::Excused);
    CHECK(!crew.store.attendanceSummary(crew.ana).rate());
}

TEST(set_mark_sets_changes_and_clears) {
    Crew crew;
    const std::int64_t practice = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(practice, crew.ana, Status::Present);
    crew.store.setMark(practice, crew.ana, Status::Late);   // replaces, does not add a second mark
    CHECK_EQ(crew.store.attendanceSummary(crew.ana).late, 1);
    CHECK_EQ(crew.store.attendanceSummary(crew.ana).present, 0);
    crew.store.setMark(practice, crew.ana, std::nullopt);
    CHECK_EQ(crew.store.practiceSummary(practice).marked(), 0);
}

TEST(mark_rest_only_fills_in_athletes_without_a_mark) {
    Crew crew;
    const std::int64_t practice = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(practice, crew.ana, Status::Absent);
    CHECK_EQ(crew.store.markRest(practice, Status::Present), 2);
    const PracticeSummary summary = crew.store.practiceSummary(practice);
    CHECK_EQ(summary.absent, 1);
    CHECK_EQ(summary.present, 2);
    CHECK_EQ(summary.onWater(), 2);
    CHECK_EQ(summary.total, 3);
    CHECK_EQ(crew.store.markRest(practice, Status::Present), 0);
    CHECK_EQ(crew.store.markRest(9999, Status::Present), 0);
}

TEST(inactive_athletes_leave_new_sheets_but_keep_old_marks) {
    Crew crew;
    const std::int64_t before = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(before, crew.ana, Status::Present);
    Athlete ana = *crew.store.athlete(crew.ana);
    ana.active = false;
    crew.store.updateAthlete(ana);
    const std::int64_t after = crew.store.addPractice("2026-03-08", "");
    CHECK_EQ(crew.store.attendanceSheet(before).size(), 3u);
    CHECK_EQ(crew.store.attendanceSheet(after).size(), 2u);
}

TEST(recent_marks_lists_the_latest_practices_oldest_first) {
    Crew crew;
    const std::int64_t first = crew.store.addPractice("2026-03-01", "");
    const std::int64_t second = crew.store.addPractice("2026-03-08", "");
    crew.store.addPractice("2026-03-15", "");
    crew.store.setMark(first, crew.ana, Status::Absent);
    crew.store.setMark(second, crew.ana, Status::Present);
    const std::vector<RecentMark> marks = crew.store.recentMarks(crew.ana, 2);
    CHECK_EQ(marks.size(), 2u);
    CHECK_EQ(marks.at(0).date, "2026-03-08");
    CHECK(marks.at(0).status == Status::Present);
    CHECK_EQ(marks.at(1).date, "2026-03-15");
    CHECK(!marks.at(1).status);
}

TEST(practices_need_a_real_date_and_list_newest_first) {
    Crew crew;
    CHECK_THROWS(crew.store.addPractice("next week", ""), ValidationError);
    crew.store.addPractice("2026-03-01", "Starts");
    crew.store.addPractice("2026-03-08", "Race pieces");
    CHECK_EQ(crew.store.practices().at(0).title, "Race pieces");
}

// ---- Results and rankings ----

TEST(ranking_puts_the_lowest_time_first_and_shares_ranks_on_ties) {
    Crew crew;
    const std::int64_t t = crew.test500();
    crew.store.addResult(crew.ana, t, "2026-03-01", 130);
    crew.store.addResult(crew.ana, t, "2026-03-08", 126.5);
    crew.store.addResult(crew.bo, t, "2026-03-01", 124);
    crew.store.addResult(crew.cy, t, "2026-03-08", 126.5);
    crew.store.addResult(crew.add("Di", Side::Left), t, "2026-03-08", 140);

    const std::vector<RankingRow> rows = crew.store.ranking(t);
    CHECK_EQ(names(rows), "Bo, Ana, Cy, Di");
    CHECK_EQ(rows.at(0).rank, 1);
    CHECK_EQ(rows.at(1).rank, 2);
    CHECK_EQ(rows.at(2).rank, 2);
    CHECK_EQ(rows.at(3).rank, 4);
    CHECK_EQ(rows.at(1).best, 126.5);
    CHECK_EQ(rows.at(1).gap, 2.5);
    CHECK_EQ(rows.at(1).latest, 126.5);
    CHECK_EQ(rows.at(1).latestDate, "2026-03-08");
    CHECK_EQ(rows.at(1).count, 2);
}

TEST(ranking_uses_the_best_result_even_when_the_latest_is_worse) {
    Crew crew;
    const std::int64_t t = crew.test500();
    crew.store.addResult(crew.ana, t, "2026-03-01", 120);
    crew.store.addResult(crew.ana, t, "2026-03-08", 125);
    const RankingRow row = crew.store.ranking(t).at(0);
    CHECK_EQ(row.best, 120.0);
    CHECK_EQ(row.latest, 125.0);
}

TEST(ranking_puts_the_highest_first_for_higher_is_better_tests) {
    Crew crew;
    const std::int64_t pullUps = crew.store.addTest("Pull-ups", TestKind::Number, "reps", false);
    crew.store.addResult(crew.ana, pullUps, "2026-03-01", 12);
    crew.store.addResult(crew.bo, pullUps, "2026-03-01", 15);
    const std::vector<RankingRow> rows = crew.store.ranking(pullUps);
    CHECK_EQ(names(rows), "Bo, Ana");
    CHECK_EQ(rows.at(1).gap, 3.0);
}

TEST(ranking_leaves_out_inactive_athletes_and_other_tests) {
    Crew crew;
    crew.store.addResult(crew.ana, crew.test500(), "2026-03-01", 120);
    crew.store.addResult(crew.bo, crew.test500(), "2026-03-01", 125);
    crew.store.addResult(crew.cy, crew.test200(), "2026-03-01", 50);
    Athlete ana = *crew.store.athlete(crew.ana);
    ana.active = false;
    crew.store.updateAthlete(ana);
    CHECK_EQ(names(crew.store.ranking(crew.test500())), "Bo");
    CHECK_EQ(crew.store.ranking(9999).size(), 0u);
}

TEST(time_tests_are_always_lower_is_better) {
    Crew crew;
    const std::int64_t id = crew.store.addTest("1 km", TestKind::Time, "ignored", false);
    CHECK(crew.store.test(id)->lowerIsBetter);
    CHECK_EQ(crew.store.test(id)->unit, "");
}

TEST(results_are_validated) {
    Crew crew;
    CHECK_THROWS(crew.store.addResult(crew.ana, crew.test500(), "yesterday", 120), ValidationError);
    CHECK_THROWS(crew.store.addResult(crew.ana, crew.test500(), "2026-03-01", -1), ValidationError);
    CHECK_THROWS(crew.store.addResult(9999, crew.test500(), "2026-03-01", 120), ValidationError);
    CHECK_THROWS(crew.store.addResult(crew.ana, 9999, "2026-03-01", 120), ValidationError);
    CHECK_EQ(crew.store.counts().results, 0);
}

// ---- Deleting ----

TEST(deleting_an_athlete_removes_their_marks_and_results) {
    Crew crew;
    const std::int64_t practice = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(practice, crew.ana, Status::Present);
    crew.store.setMark(practice, crew.bo, Status::Present);
    crew.store.addResult(crew.ana, crew.test500(), "2026-03-01", 120);
    crew.store.deleteAthlete(crew.ana);
    CHECK(!crew.store.athlete(crew.ana));
    CHECK_EQ(crew.store.practiceSummary(practice).marked(), 1);
    CHECK_EQ(crew.store.counts().results, 0);
}

TEST(deleting_a_test_removes_its_results) {
    Crew crew;
    crew.store.addResult(crew.ana, crew.test500(), "2026-03-01", 120);
    crew.store.addResult(crew.ana, crew.test200(), "2026-03-01", 50);
    crew.store.deleteTest(crew.test500());
    CHECK_EQ(crew.store.tests().size(), 2u);
    CHECK_EQ(crew.store.counts().results, 1);
}

TEST(deleting_a_practice_removes_its_marks) {
    Crew crew;
    const std::int64_t practice = crew.store.addPractice("2026-03-01", "");
    crew.store.setMark(practice, crew.ana, Status::Absent);
    crew.store.deletePractice(practice);
    CHECK_EQ(crew.store.attendanceSummary(crew.ana).absent, 0);
    CHECK_EQ(crew.store.counts().practices, 0);
}

TEST(erase_all_returns_to_an_empty_team) {
    Crew crew;
    crew.store.setTeamName("River Dragons");
    crew.store.addPractice("2026-03-01", "");
    crew.store.eraseAll();
    const Counts counts = crew.store.counts();
    CHECK_EQ(counts.athletes + counts.practices + counts.results, 0);
    CHECK_EQ(crew.store.tests().size(), 3u);
    CHECK_EQ(crew.store.teamName(), "My crew");
}

// ---- Storage ----

TEST(a_failed_transaction_changes_nothing) {
    Crew crew;
    try {
        crew.store.inTransaction([&] {
            crew.add("Di", Side::Left);
            crew.store.addResult(crew.ana, crew.test500(), "not a date", 120);   // throws
        });
    } catch (const ValidationError&) {
    }
    CHECK_EQ(crew.store.athletes().size(), 3u);
}

TEST(the_database_rejects_values_the_schema_forbids) {
    Database db(":memory:");
    TeamStore store(db);
    CHECK_THROWS(db.execute("INSERT INTO athletes (name, side, role) VALUES ('X', 'Q', 'paddler')"), DatabaseError);
    CHECK_THROWS(db.execute("INSERT INTO attendance (practice_id, athlete_id, status) VALUES (1, 1, 'P')"), DatabaseError);
    CHECK_THROWS(db.prepare("SELECT * FROM no_such_table"), DatabaseError);
}

TEST(data_survives_closing_and_reopening_the_file) {
    const std::string path = "ptt_test_reopen.db";
    std::remove(path.c_str());
    {
        Database db(path);
        TeamStore store(db);
        Athlete ana;
        ana.name = "Ana";
        const std::int64_t id = store.addAthlete(ana);
        store.addResult(id, store.tests().at(1).id, "2026-03-01", 125.3);
        store.setTeamName("River Dragons");
    }
    {
        Database db(path);
        TeamStore store(db);   // must not recreate tables or add the default tests again
        CHECK_EQ(store.athletes().size(), 1u);
        CHECK_EQ(store.tests().size(), 3u);
        CHECK_EQ(store.counts().results, 1);
        CHECK_EQ(store.teamName(), "River Dragons");
    }
    std::remove(path.c_str());
}

// ---- Sample crew and CSV ----

TEST(the_sample_crew_loads_and_replaces_existing_data) {
    Crew crew;
    loadSampleCrew(crew.store, "2026-06-15");
    const Counts counts = crew.store.counts();
    CHECK_EQ(counts.athletes, 16);
    CHECK_EQ(counts.practices, 8);
    CHECK_EQ(counts.results, 14 * 4);
    CHECK_EQ(crew.store.teamName(), "Sample crew");
    CHECK_EQ(crew.store.ranking(crew.test500()).size(), 14u);
    CHECK_EQ(crew.store.practices().at(0).date, "2026-06-13");
    CHECK_EQ(crew.store.practices().at(7).date, "2026-05-20");
}

TEST(csv_cells_are_quoted_and_formulas_defused) {
    CHECK_EQ(csvCell("plain"), "plain");
    CHECK_EQ(csvCell("Lee, Ana"), "\"Lee, Ana\"");
    CHECK_EQ(csvCell("says \"go\""), "\"says \"\"go\"\"\"");
    CHECK_EQ(csvCell("=SUM(A1)"), "'=SUM(A1)");
    CHECK_EQ(csvCell(""), "");
}

TEST(csv_exports_have_one_row_per_athlete_mark_and_result) {
    Crew crew;
    loadSampleCrew(crew.store, "2026-06-15");
    std::ostringstream roster, attendance, results;
    exportRosterCsv(crew.store, roster);
    exportAttendanceCsv(crew.store, attendance);
    exportResultsCsv(crew.store, results);
    CHECK_EQ(countLines(roster.str()), 1 + 16);
    CHECK_EQ(countLines(attendance.str()), 1 + 16 * 8);
    CHECK_EQ(countLines(results.str()), 1 + 14 * 4);
    CHECK(contains(roster.str(), "Name,Side,Role,Gender,Weight (lb),Active,Attendance %,Notes\r\n"));
    CHECK(contains(roster.str(), "Amara Osei,Left,Paddler,F,148,Yes,"));
}

// ---- The menus, driven by scripted input ----

TEST(a_scripted_session_through_the_menus) {
    Database db(":memory:");
    TeamStore store(db);
    std::istringstream input(
        "1\n"                   // Roster
        "1\n"                   //   Add athlete
        "Ana Lee\nL\nP\nF\n150\n-\n"
        "1\n"                   //   Add athlete
        "Bo Park\nR\n\n-\n-\nstrong finisher\n"
        "1\n"                   //   Add athlete, with a bad side: must not be saved
        "Nobody\nX\n"
        "0\n"                   // Back
        "2\n"                   // Practices
        "1\n"                   //   New practice
        "2026-03-01\nStarts\n"
        "1 L\n"                 //     Ana late
        "rest P\n"              //     Bo present
        "9 A\n"                 //     no athlete 9: ignored with a message
        "\n"                    //     finish
        "0\n"                   // Back
        "3\n"                   // Performance (200 m time trial is first)
        "1\n"                   //   Log a session
        "2026-03-01\n"
        "48.5\n"                //     Ana
        "nope\n47.25\n"         //     Bo: a typo, then a valid time
        "0\n"                   // Back
        "0\n");                 // Quit
    std::ostringstream output;
    App app(store, input, output);
    app.run();
    const std::string text = output.str();

    CHECK_EQ(store.athletes().size(), 2u);
    CHECK(contains(text, "Not saved: Side must be L, R or B."));
    const Athlete bo = store.athletes().at(1);
    CHECK_EQ(bo.name, "Bo Park");
    CHECK(bo.role == Role::Paddler);
    CHECK_EQ(bo.notes, "strong finisher");
    CHECK(!bo.weight);

    const std::int64_t practice = store.practices().at(0).id;
    const PracticeSummary summary = store.practiceSummary(practice);
    CHECK_EQ(summary.late, 1);
    CHECK_EQ(summary.present, 1);
    CHECK(contains(text, "2 of 2 on the water, 1 late"));
    CHECK(contains(text, "\"9\" is not a number from 1 to 2."));

    const std::vector<RankingRow> ranking = store.ranking(store.tests().at(0).id);
    CHECK_EQ(names(ranking), "Bo Park, Ana Lee");
    CHECK(contains(text, "That is not a valid result."));
    CHECK(contains(text, "Saved 2 results."));
    CHECK(contains(text, "+1.25"));
}

TEST(running_out_of_input_ends_the_session_cleanly) {
    Database db(":memory:");
    TeamStore store(db);
    std::istringstream input("1\n1\nAna\n");   // stops halfway through adding an athlete
    std::ostringstream output;
    App app(store, input, output);
    CHECK_THROWS(app.run(), EndOfInput);
    CHECK_EQ(store.athletes().size(), 0u);
}

int main() {
    return testing::runAllTests();
}
