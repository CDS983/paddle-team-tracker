// Paddle Team Tracker: roster, attendance and performance tracking for
// dragon boat and outrigger crews.
//
// Usage:  paddle-team-tracker [database-file]
// The team is stored in team.db in the current folder unless another file is named.

#include "app.h"
#include "../core/database.h"
#include "../core/team_store.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const std::string path = argc > 1 ? argv[1] : "team.db";
    try {
        ptt::Database database(path);
        ptt::TeamStore store(database);
        ptt::App app(store, std::cin, std::cout);
        app.run();
    } catch (const ptt::EndOfInput&) {
        // Input was closed (Ctrl+Z or Ctrl+D). Everything is already saved.
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
