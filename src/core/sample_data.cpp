#include "sample_data.h"

#include "format.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace ptt {

namespace {

struct SamplePerson {
    const char* name;
    Side side;
    Role role;
    const char* gender;
    double weight;
};

// The names are invented.
const SamplePerson kPeople[] = {
    {"Amara Osei", Side::Left, Role::Paddler, "F", 148},     {"Ben Castellano", Side::Right, Role::Paddler, "M", 182},
    {"Chloe Nakagawa", Side::Left, Role::Paddler, "F", 136}, {"Dario Mwangi", Side::Right, Role::Paddler, "M", 176},
    {"Elena Brandt", Side::Both, Role::Paddler, "F", 152},   {"Farid Halloran", Side::Left, Role::Paddler, "M", 190},
    {"Grace Tupou", Side::Right, Role::Paddler, "F", 160},   {"Hugo Lindqvist", Side::Left, Role::Paddler, "M", 171},
    {"Imani Ferreira", Side::Right, Role::Paddler, "F", 143}, {"Jonah Whitfield", Side::Both, Role::Paddler, "M", 185},
    {"Keiko Marlowe", Side::Left, Role::Paddler, "F", 139},  {"Luca Abernathy", Side::Right, Role::Paddler, "M", 178},
    {"Mina Okonkwo", Side::Left, Role::Paddler, "F", 150},   {"Nico Varga", Side::Right, Role::Paddler, "M", 188},
    {"Odette Pham", Side::Both, Role::Drummer, "F", 118},    {"Pavel Iwata", Side::Both, Role::Steer, "M", 165},
};

// A tiny random number generator with a fixed starting point, so the sample
// is identical every time it loads. Returns a value from 0 up to (not including) 1.
class Random {
public:
    double next() {
        state_ = (state_ * 1103515245u + 12345u) % 2147483648u;
        return static_cast<double>(state_) / 2147483648.0;
    }

private:
    std::uint64_t state_ = 7;
};

double roundToHundredths(double value) {
    return std::round(value * 100) / 100;
}

}  // namespace

void loadSampleCrew(TeamStore& store, const std::string& today) {
    store.eraseAll();
    store.inTransaction([&] {
        store.setTeamName("Sample crew");
        Random random;

        std::vector<Athlete> crew;
        for (const SamplePerson& person : kPeople) {
            Athlete athlete;
            athlete.name = person.name;
            athlete.side = person.side;
            athlete.role = person.role;
            athlete.gender = person.gender;
            athlete.weight = person.weight;
            athlete.id = store.addAthlete(athlete);
            crew.push_back(athlete);
        }

        // Eight practices, every three or four days, ending a couple of days ago.
        const char* titles[] = {"Technique", "Race pieces", "Long paddle", "Starts",
                                "Time trials", "Race pieces", "Technique", "Long paddle"};
        for (int i = 0; i < 8; ++i) {
            const int daysAgo = 26 - (i * 7) / 2;
            const std::int64_t practiceId = store.addPractice(addDays(today, -daysAgo), titles[i]);
            for (const Athlete& athlete : crew) {
                const double roll = random.next();
                const Status status = roll < 0.78 ? Status::Present
                                    : roll < 0.86 ? Status::Late
                                    : roll < 0.95 ? Status::Absent
                                                  : Status::Excused;
                store.setMark(practiceId, athlete.id, status);
            }
        }

        // Two rounds of 500 m and 200 m time trials for the paddlers, with
        // the second round a little faster than the first.
        const std::vector<Test> tests = store.tests();
        const std::int64_t test200 = tests.at(0).id;
        const std::int64_t test500 = tests.at(1).id;
        for (const Athlete& athlete : crew) {
            if (athlete.role != Role::Paddler) continue;
            const double base = (athlete.gender == "M" ? 118.0 : 131.0) + random.next() * 9;
            const int roundDaysAgo[] = {24, 10};
            const double improvement[] = {0.0, -1.2};
            for (int round = 0; round < 2; ++round) {
                const std::string date = addDays(today, -roundDaysAgo[round]);
                const double time500 = roundToHundredths(base + improvement[round] + random.next() * 1.5);
                const double time200 = roundToHundredths(time500 * 0.39 + random.next());
                store.addResult(athlete.id, test500, date, time500);
                store.addResult(athlete.id, test200, date, time200);
            }
        }
    });
}

}  // namespace ptt
