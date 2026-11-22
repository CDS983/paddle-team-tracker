// Helpers for a text interface: asking questions, reading answers, and
// printing tables.
//
// Console reads from and writes to streams handed to it, not std::cin and
// std::cout directly. The real program passes those two; the tests pass
// string streams, which lets them "type" a whole session and check the output.
#pragma once

#include <exception>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace ptt {

// Thrown when the input runs out (Ctrl+Z on Windows, Ctrl+D elsewhere, or the
// end of a scripted test). main() catches it and exits normally.
class EndOfInput : public std::exception {
public:
    const char* what() const noexcept override { return "end of input"; }
};

class Console {
public:
    Console(std::istream& in, std::ostream& out);

    // Shows the prompt and returns what was typed, trimmed.
    std::string ask(const std::string& prompt);
    // Like ask(), but pressing Enter keeps the current value, shown in brackets.
    std::string askKeeping(const std::string& prompt, const std::string& current);
    // Repeats the question until a whole number from min to max is entered.
    int askNumber(const std::string& prompt, int min, int max);
    // Like askNumber(), but pressing Enter cancels and returns nothing.
    std::optional<int> askNumberOrCancel(const std::string& prompt, int min, int max);
    // A yes/no question. Only "y" or "yes" counts as yes.
    bool confirm(const std::string& question);
    void pause();

    void line(const std::string& text = "");
    void heading(const std::string& text);

    // Prints numbered choices plus "0. Back" (or another label for 0) and returns the choice.
    int menu(const std::vector<std::string>& choices, const std::string& zeroLabel = "Back");

    // Prints rows in aligned columns. Columns listed in `rightAligned` (by
    // index) are lined up on the right, which suits numbers.
    void table(const std::vector<std::string>& headers,
               const std::vector<std::vector<std::string>>& rows,
               const std::vector<std::size_t>& rightAligned = {});

private:
    std::istream& in_;
    std::ostream& out_;
};

}  // namespace ptt
