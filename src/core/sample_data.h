// A made-up crew, so the program can be explored before real data goes in.
#pragma once

#include "team_store.h"

#include <string>

namespace ptt {

// Replaces everything in the store with 16 invented athletes, 8 practices with
// attendance, and two rounds of time trials. Dates are placed in the weeks
// before `today` ("YYYY-MM-DD").
void loadSampleCrew(TeamStore& store, const std::string& today);

}  // namespace ptt
