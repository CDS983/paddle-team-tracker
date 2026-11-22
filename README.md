# Paddle Team Tracker

A roster, attendance and performance tracker for dragon boat and outrigger crews, written in C++17 with a SQLite database. It runs in a terminal.

## Why I built this: 
Throughout my entire paddling career there has never been any kind of uniform or standard system for organizing and tracking paddler statistics. I have seen coaches simply make rosters based on spreadsheets that could crash your device if your phone wasn't strong enough to handle them, simply through sight, or going off of statistics written down on a white board. My goal was to begin creating a system that would make it easier for coaches and training staff to catalog their data and quickly rank paddlers. As of now the app allows for staff to catalog speed, weight, gender, preferred paddling side, and their attendance. 

```
Performance: 200 m time trial
=============================
Rank  Athlete          Side    Best    Gap              Latest  Results
----  ---------------  -----  -----  -----  ------------------  -------
   1  Luca Abernathy   Right  46.65         46.65 (2026-11-06)        2
   2  Hugo Lindqvist   Left   46.85  +0.20  46.91 (2026-11-06)        2
   3  Farid Halloran   Left   47.76  +1.11  47.76 (2026-11-06)        2
   4  Nico Varga       Right  48.45  +1.80  48.45 (2026-11-06)        2

Ranked by each active athlete's best result. Lower is better.
```
- **Names and statistics in the sample file are fully made up and do not represent any real person. 
## What it does

- **Roster**: each athlete's side (left, right or both), role, gender, weight and notes. Athletes can also be made inactive, this will make it so their data stay but they won't be counted towards rosters.
- **Attendance**: create a practice and mark athletes present, late, absent or excused. Several athletes can be marked in one line (`2 5 L`), and `rest P` marks everyone left as present. Each athlete gets an attendance percentage and a record of their last 10 practices.
- **Performance**: log a whole session of time-trial results at once and see the crew ranked by best result, with the difference in time to the leader. 200m, 500m, and 2000m tests are listed but the user is able to add whatever distance or type of metric they want to add.
- **Team data**: export the roster, attendance and results as CSV files for Excel or Google Sheets, load the sample crew, or erase everything.

Everything is stored in a single file, `team.db`, in the folder the program is started from.

## Build and run


### Visual Studio (Windows)

1. Install Visual Studio with the "Desktop development with C++" workload, which includes CMake support.
2. Choose **File > Open > Folder** and pick this folder. Visual Studio reads `CMakeLists.txt` and sets the project up.
3. In the startup item dropdown next to the green Run button, choose `paddle-team-tracker.exe`, then run it.

### Command line (Windows)

```
cmake -S . -B build
cmake --build build
```

Then run the program. With Visual Studio's compiler it is at `build\Debug\paddle-team-tracker.exe`; with other compilers it is at `build/paddle-team-tracker`.

To use a different database file, pass its path: `paddle-team-tracker my-team.db`.

To look around with data already in place, choose **Team data**, then **Load sample crew**.

## Run the tests

The build also produces `ptt_tests`, which checks the calculations, the database rules, and a scripted run through the menus:

```
ctest --test-dir build --output-on-failure
```

or run `ptt_tests` directly to see each test by name. In Visual Studio the tests appear in **Test > Test Explorer**.

Built and tested with Visual Studio 2026


## How the code is organized

```
src/core/     Data, storage and calculations. No input or output.
  database    A small RAII wrapper around SQLite's C API
  models      The plain data types: Athlete, Practice, Test, Result
  team_store  Every query and every change, and the only file containing SQL
  format      Parsing and formatting of times, numbers and dates
  csv         CSV export
  sample_data The sample crew
src/app/      The console interface
  console     Asking questions, reading answers, printing tables
  app         The menus
  main        Opens the database and starts the menus
tests/        A minimal test framework and the tests
third_party/  SQLite (public domain)
```

`docs/ARCHITECTURE.md` explains the design: the database schema, how a request travels through the layers, and the reasons behind the main decisions.

