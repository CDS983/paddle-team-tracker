// Writes the team's data as CSV, the plain-text table format that Excel and
// Google Sheets open.
#pragma once

#include "team_store.h"

#include <ostream>
#include <string>
#include <vector>

namespace ptt {

// Prepares one value for a CSV file: wraps it in quotes when it contains a
// comma, quote or line break, and stops spreadsheets from running it as a formula.
std::string csvCell(const std::string& value);

void writeCsvRow(std::ostream& out, const std::vector<std::string>& cells);

void exportRosterCsv(TeamStore& store, std::ostream& out);
void exportAttendanceCsv(TeamStore& store, std::ostream& out);
void exportResultsCsv(TeamStore& store, std::ostream& out);

}  // namespace ptt
