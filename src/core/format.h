// Turning text typed by a person into values, and values back into text:
// race times, plain numbers, dates and percentages.
#pragma once

#include "models.h"

#include <optional>
#include <string>

namespace ptt {

// Removes spaces and tabs from both ends.
std::string trim(const std::string& text);

// ---- Times, stored as seconds ----

// Reads "58.4", "2:05.3" or "1:02:03.5". Returns seconds rounded to
// hundredths, or nothing if the text is not a time greater than zero.
std::optional<double> parseTime(const std::string& text);

// Writes seconds as "58.40", "2:05.30" or "1:02:03.50".
std::string formatTime(double seconds);

// ---- Plain numbers ----

// Reads a non-negative number such as "14" or "72.5".
std::optional<double> parseNumber(const std::string& text);

// Writes a number with at most two decimals and no trailing zeros.
std::string formatNumber(double value);

// ---- Results, which are a time or a number depending on the test ----

std::optional<double> parseResult(const Test& test, const std::string& text);
std::string formatResult(const Test& test, double value);

// "88%", or "-" when there is nothing to report.
std::string formatPercent(const std::optional<double>& rate);

// ---- Dates, stored as "YYYY-MM-DD" ----

bool isValidDate(const std::string& text);
std::string todayIso();
// The date `days` after (or, if negative, before) the given one.
std::string addDays(const std::string& isoDate, int days);

}  // namespace ptt
