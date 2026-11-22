#include "format.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>

namespace ptt {

namespace {

bool allDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(),
                                        [](unsigned char c) { return std::isdigit(c) != 0; });
}

// Accepts digits with an optional fraction, such as "5", "05.3" or "72.50".
bool isDecimal(const std::string& text) {
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos) return allDigits(text);
    return allDigits(text.substr(0, dot)) && allDigits(text.substr(dot + 1));
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parts;
}

// People in some countries type a comma for the decimal point.
std::string normalise(const std::string& text) {
    std::string cleaned = trim(text);
    std::replace(cleaned.begin(), cleaned.end(), ',', '.');
    return cleaned;
}

std::tm localTime(std::time_t when) {
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &when);
#else
    localtime_r(&when, &parts);
#endif
    return parts;
}

std::string isoFromTm(const std::tm& parts) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02d", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday);
    return buffer;
}

}  // namespace

std::string trim(const std::string& text) {
    const char* blanks = " \t\r\n";
    const std::size_t first = text.find_first_not_of(blanks);
    if (first == std::string::npos) return {};
    const std::size_t last = text.find_last_not_of(blanks);
    return text.substr(first, last - first + 1);
}

std::optional<double> parseTime(const std::string& text) {
    const std::vector<std::string> parts = split(normalise(text), ':');
    if (parts.size() > 3) return std::nullopt;

    // Every part except the last is a whole number (hours or minutes).
    // The last part is seconds and may have a fraction.
    double seconds = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const bool isLast = i + 1 == parts.size();
        if (isLast ? !isDecimal(parts[i]) : !allDigits(parts[i])) return std::nullopt;
        const double value = std::stod(parts[i]);
        // After the first part, 60 or more would mean the time was typed wrong ("2:75").
        if (i > 0 && value >= 60) return std::nullopt;
        seconds = seconds * 60 + value;
    }
    seconds = std::round(seconds * 100) / 100;
    if (seconds <= 0) return std::nullopt;
    return seconds;
}

std::string formatTime(double seconds) {
    // Work in whole hundredths so rounding cannot produce "1:60.00".
    const long long hundredths = std::llround(seconds * 100);
    const long long wholeSeconds = hundredths / 100;
    const int cs = static_cast<int>(hundredths % 100);
    const int s = static_cast<int>(wholeSeconds % 60);
    const int m = static_cast<int>((wholeSeconds / 60) % 60);
    const long long h = wholeSeconds / 3600;

    char buffer[32];
    if (h > 0) {
        std::snprintf(buffer, sizeof buffer, "%lld:%02d:%02d.%02d", h, m, s, cs);
    } else if (m > 0) {
        std::snprintf(buffer, sizeof buffer, "%d:%02d.%02d", m, s, cs);
    } else {
        std::snprintf(buffer, sizeof buffer, "%d.%02d", s, cs);
    }
    return buffer;
}

std::optional<double> parseNumber(const std::string& text) {
    const std::string cleaned = normalise(text);
    if (!isDecimal(cleaned)) return std::nullopt;
    return std::stod(cleaned);
}

std::string formatNumber(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f", value);
    std::string text = buffer;
    // "72.50" -> "72.5", "14.00" -> "14"
    text.erase(text.find_last_not_of('0') + 1);
    if (text.back() == '.') text.pop_back();
    return text;
}

std::optional<double> parseResult(const Test& test, const std::string& text) {
    return test.kind == TestKind::Time ? parseTime(text) : parseNumber(text);
}

std::string formatResult(const Test& test, double value) {
    if (test.kind == TestKind::Time) return formatTime(value);
    return test.unit.empty() ? formatNumber(value) : formatNumber(value) + " " + test.unit;
}

std::string formatPercent(const std::optional<double>& rate) {
    if (!rate) return "-";
    return std::to_string(std::lround(*rate * 100)) + "%";
}

bool isValidDate(const std::string& text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
    const std::string year = text.substr(0, 4), month = text.substr(5, 2), day = text.substr(8, 2);
    if (!allDigits(year) || !allDigits(month) || !allDigits(day)) return false;

    const int y = std::stoi(year), m = std::stoi(month), d = std::stoi(day);
    if (y < 1900 || m < 1 || m > 12 || d < 1) return false;
    const bool leapYear = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    const int daysInMonth[] = {31, leapYear ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return d <= daysInMonth[m - 1];
}

std::string todayIso() {
    return isoFromTm(localTime(std::time(nullptr)));
}

std::string addDays(const std::string& isoDate, int days) {
    if (!isValidDate(isoDate)) return isoDate;
    std::tm parts{};
    parts.tm_year = std::stoi(isoDate.substr(0, 4)) - 1900;
    parts.tm_mon = std::stoi(isoDate.substr(5, 2)) - 1;
    parts.tm_mday = std::stoi(isoDate.substr(8, 2)) + days;
    parts.tm_hour = 12;    // midday, so a daylight-saving change cannot shift the date
    parts.tm_isdst = -1;   // let the system work out daylight saving
    // mktime tidies up out-of-range values, e.g. day 35 of January becomes 4 February.
    const std::time_t when = std::mktime(&parts);
    return isoFromTm(localTime(when));
}

}  // namespace ptt
