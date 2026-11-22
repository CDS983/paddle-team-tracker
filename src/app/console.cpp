#include "console.h"

#include "../core/format.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace ptt {

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Reads a whole number, or nothing if the text is not one.
std::optional<int> toInt(const std::string& text) {
    if (text.empty() || text.size() > 9) return std::nullopt;
    if (!std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
        return std::nullopt;
    }
    return std::stoi(text);
}

}  // namespace

Console::Console(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

std::string Console::ask(const std::string& prompt) {
    out_ << prompt << ' ' << std::flush;
    std::string answer;
    if (!std::getline(in_, answer)) throw EndOfInput();
    return trim(answer);
}

std::string Console::askKeeping(const std::string& prompt, const std::string& current) {
    const std::string answer = ask(prompt + " [" + current + "]:");
    return answer.empty() ? current : answer;
}

int Console::askNumber(const std::string& prompt, int min, int max) {
    while (true) {
        const std::optional<int> number = toInt(ask(prompt));
        if (number && *number >= min && *number <= max) return *number;
        line("Enter a number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

std::optional<int> Console::askNumberOrCancel(const std::string& prompt, int min, int max) {
    while (true) {
        const std::string answer = ask(prompt);
        if (answer.empty()) return std::nullopt;
        const std::optional<int> number = toInt(answer);
        if (number && *number >= min && *number <= max) return number;
        line("Enter a number from " + std::to_string(min) + " to " + std::to_string(max) + ", or press Enter to cancel.");
    }
}

bool Console::confirm(const std::string& question) {
    const std::string answer = lower(ask(question + " (y/n)"));
    return answer == "y" || answer == "yes";
}

void Console::pause() {
    ask("Press Enter to continue.");
}

void Console::line(const std::string& text) {
    out_ << text << '\n';
}

void Console::heading(const std::string& text) {
    out_ << '\n' << text << '\n' << std::string(text.size(), '=') << '\n';
}

int Console::menu(const std::vector<std::string>& choices, const std::string& zeroLabel) {
    line();
    for (std::size_t i = 0; i < choices.size(); ++i) {
        line("  " + std::to_string(i + 1) + ". " + choices[i]);
    }
    line("  0. " + zeroLabel);
    return askNumber("Choose:", 0, static_cast<int>(choices.size()));
}

void Console::table(const std::vector<std::string>& headers,
                    const std::vector<std::vector<std::string>>& rows,
                    const std::vector<std::size_t>& rightAligned) {
    // Each column is as wide as its longest cell.
    std::vector<std::size_t> widths(headers.size());
    for (std::size_t c = 0; c < headers.size(); ++c) widths[c] = headers[c].size();
    for (const auto& row : rows) {
        for (std::size_t c = 0; c < row.size() && c < widths.size(); ++c) {
            widths[c] = std::max(widths[c], row[c].size());
        }
    }

    const auto printRow = [&](const std::vector<std::string>& cells) {
        std::string text;
        for (std::size_t c = 0; c < widths.size(); ++c) {
            const std::string cell = c < cells.size() ? cells[c] : "";
            const std::string padding(widths[c] - cell.size(), ' ');
            const bool right = std::find(rightAligned.begin(), rightAligned.end(), c) != rightAligned.end();
            text += right ? padding + cell : cell + padding;
            if (c + 1 < widths.size()) text += "  ";
        }
        // Drop the padding after the last cell so lines have no trailing spaces.
        text.erase(text.find_last_not_of(' ') + 1);
        line(text);
    };

    printRow(headers);
    std::vector<std::string> rule;
    for (const std::size_t width : widths) rule.push_back(std::string(width, '-'));
    printRow(rule);
    for (const auto& row : rows) printRow(row);
}

}  // namespace ptt
