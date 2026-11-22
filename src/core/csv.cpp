#include "csv.h"

#include "format.h"

namespace ptt {

std::string csvCell(const std::string& value) {
    std::string text = value;
    // A cell starting with = + - or @ is treated as a formula by spreadsheet
    // programs. A leading apostrophe makes them show it as plain text instead.
    if (!text.empty() && std::string("=+-@").find(text.front()) != std::string::npos) {
        text.insert(text.begin(), '\'');
    }
    if (text.find_first_of(",\"\r\n") == std::string::npos) return text;

    std::string quoted = "\"";
    for (const char c : text) {
        if (c == '"') quoted += '"';   // a quote inside a quoted cell is written twice
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

void writeCsvRow(std::ostream& out, const std::vector<std::string>& cells) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) out << ',';
        out << csvCell(cells[i]);
    }
    out << "\r\n";   // the line ending the CSV standard asks for
}

void exportRosterCsv(TeamStore& store, std::ostream& out) {
    writeCsvRow(out, {"Name", "Side", "Role", "Gender", "Weight (" + store.weightUnit() + ")", "Active", "Attendance %", "Notes"});
    for (const Athlete& athlete : store.athletes(true)) {
        const std::optional<double> rate = store.attendanceSummary(athlete.id).rate();
        writeCsvRow(out, {
            athlete.name,
            label(athlete.side),
            label(athlete.role),
            athlete.gender,
            athlete.weight ? formatNumber(*athlete.weight) : "",
            athlete.active ? "Yes" : "No",
            rate ? formatNumber(*rate * 100) : "",
            athlete.notes,
        });
    }
}

void exportAttendanceCsv(TeamStore& store, std::ostream& out) {
    writeCsvRow(out, {"Date", "Practice", "Athlete", "Status"});
    for (const AttendanceRecord& record : store.allAttendance()) {
        writeCsvRow(out, {record.date, record.practiceTitle, record.athleteName, label(record.status)});
    }
}

void exportResultsCsv(TeamStore& store, std::ostream& out) {
    writeCsvRow(out, {"Date", "Test", "Athlete", "Result", "Value (seconds for times)"});
    for (const ResultRecord& record : store.allResults()) {
        writeCsvRow(out, {record.date, record.test.name, record.athleteName,
                          formatResult(record.test, record.value), formatNumber(record.value)});
    }
}

}  // namespace ptt
