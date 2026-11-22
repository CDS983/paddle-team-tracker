# How Paddle Team Tracker works

This document explains the design of the program: what the layers are, how the data is stored, and why the main decisions were made.

## The layers

```
main.cpp        opens team.db, builds the objects below, starts the menus
   |
App             the menus: prints, asks, calls TeamStore        (src/app/app.cpp)
   |   uses Console for all reading and printing                (src/app/console.cpp)
   v
TeamStore       every query and change; the only SQL            (src/core/team_store.cpp)
   |
   v
Database        RAII wrapper around SQLite's C API               (src/core/database.cpp)
   |
   v
SQLite          third_party/sqlite/sqlite3.c
```

Each layer only talks to the one below it. `App` never writes SQL, and `TeamStore` never prints or reads input. Two things follow from that:

- **The logic can be tested without a screen.** The tests create a `TeamStore` on an in-memory database and call it directly.
- **The interface can be replaced.** A window or a web front end could sit on top of the same `TeamStore` without changing it.

## The database

SQLite stores the whole team in one file. The tables, created in `TeamStore::createSchema()`:

| Table | One row per | Notes |
|---|---|---|
| `athletes` | athlete | `side`, `role` and `gender` are limited to known values by `CHECK` rules |
| `practices` | practice | `date` is text in `YYYY-MM-DD` form |
| `attendance` | athlete at a practice | primary key is the pair (`practice_id`, `athlete_id`) |
| `tests` | kind of test | a time, or a number with a unit |
| `results` | logged result | `value` is seconds for time tests |
| `settings` | setting | key and value, for the team name and weight unit |

Decisions worth knowing:

- **Attendance is its own table.** An athlete attends many practices and a practice has many athletes. A many-to-many relationship like that is stored as one row per pairing. Making the pair the primary key means the database itself guarantees an athlete cannot have two marks at one practice.
- **Foreign keys with `ON DELETE CASCADE`.** `attendance` and `results` refer to athletes, practices and tests. When one of those is deleted, SQLite removes the rows that pointed at it, so there is never a mark or result belonging to nothing. SQLite only enforces this when `PRAGMA foreign_keys = ON` is run on each connection, which `Database`'s constructor does.
- **Dates as `YYYY-MM-DD` text.** In that form, sorting alphabetically is the same as sorting by date, so `ORDER BY date` works without any date functions.
- **Times as seconds.** Storing `125.3` instead of `"2:05.3"` lets the database compare and rank results with `MIN`, `MAX` and `ORDER BY`. Conversion to and from the `m:ss.xx` form happens only at the edges, in `format.cpp`.
- **`CHECK` constraints.** The C++ code validates input, and the schema repeats the most important rules. If a bug ever tried to store a side of `'Q'`, the database would refuse it.
- **Schema version.** SQLite keeps a spare integer per file, `user_version`. `createSchema()` reads it to tell a new file (version 0, so create the tables) from an existing one. A later change to the tables would add a step that upgrades version 1 to version 2.

## The database wrapper

SQLite's API is C: functions return integer error codes and hand out pointers that must be released with matching calls. `database.h` wraps that in three small classes.

- **`Database`** owns the connection. Its destructor closes it.
- **`Statement`** owns one prepared query. Its destructor finalises it.
- **`Transaction`** begins a transaction. Unless `commit()` is called, its destructor rolls back.

This is RAII: tying a resource to an object's lifetime so cleanup happens automatically when the object goes out of scope, including when an exception is thrown. Error codes are turned into `DatabaseError` exceptions so they cannot be silently ignored.

The classes are not copyable (two objects would both try to release the same pointer). `Statement` is movable so `Database::prepare()` can return one.

### Prepared statements

Values never get pasted into SQL text. Queries are written with placeholders and the values are bound separately:

```cpp
db_.prepare("DELETE FROM athletes WHERE id = ?1").bindInt(1, id).run();
```

This is what prevents SQL injection. A name such as `Robert'); DROP TABLE athletes;--` is stored as ordinary text, and there is a test for exactly that. The one place SQL text is assembled from pieces is the ranking query, where the choice between `MIN`/`ASC` and `MAX`/`DESC` comes from fixed strings in the code, never from input.

## Following one request: marking attendance

1. In `App::takeAttendance()`, the person types `2 5 L`.
2. `App` splits the line into words. The last word is the status; the others are row numbers on the sheet it just printed. Every number is checked before anything is changed, so a typo changes nothing.
3. `App` calls `TeamStore::setMark()` for each athlete inside `inTransaction()`, so either both marks are saved or neither is.
4. `setMark()` runs an "upsert": insert the row, or if that athlete already has a mark at that practice, update it.

   ```sql
   INSERT INTO attendance (practice_id, athlete_id, status) VALUES (?1, ?2, ?3)
   ON CONFLICT (practice_id, athlete_id) DO UPDATE SET status = excluded.status
   ```
5. The loop goes round again: `attendanceSheet()` re-reads the sheet from the database and it is printed with the new marks.

The screen is always redrawn from what is stored. `App` keeps no copy of the data that could drift out of date.

## The calculations

- **Attendance rate** (`AttendanceSummary::rate()` in `models.h`): present plus late, divided by present plus late plus absent. Excused practices are left out of both halves, so an excused absence neither helps nor hurts. With nothing to count, the rate is empty and shown as `-`.
- **Attendance sheet** (`TeamStore::attendanceSheet()`): a `LEFT JOIN` from athletes to this practice's marks, which keeps athletes who have no mark yet. It lists active athletes plus anyone already marked, so making an athlete inactive removes them from future sheets without rewriting past ones.
- **Ranking** (`TeamStore::ranking()`): `GROUP BY` athlete with `MIN(value)` or `MAX(value)` gives each athlete's best result, and `ORDER BY` puts them in order. C++ then assigns ranks, giving equal results the same rank (1, 2, 2, 4), and works out each athlete's gap to the leader.
- **Time parsing** (`parseTime()` in `format.cpp`): splits on `:`, requires whole numbers for hours and minutes, allows a fraction on the seconds, and rejects parts of 60 or more after the first, so `2:75` is refused instead of becoming 3:15.

## The console layer

`Console` takes an input stream and an output stream in its constructor. The real program passes `std::cin` and `std::cout`. The tests pass string streams: one holding everything a person would type in a session, one that collects everything printed. That is how `a_scripted_session_through_the_menus` in `tests/tests.cpp` drives the real menus and then checks both the database and the output.

Two kinds of failure are handled differently:

- **`ValidationError`** means something entered was not acceptable (an empty name, an impossible date). `App::attempt()` catches it, prints the reason, and returns to the menu.
- **`DatabaseError`** means something is wrong with the storage itself. It is not caught in the menus; `main()` reports it and exits.

When input ends (Ctrl+Z on Windows), `Console` throws `EndOfInput`, which unwinds out of whatever menu was open and lets `main()` exit normally. Because every change is saved as it is made, nothing is lost.

## Testing

`tests/test_framework.h` is a test framework in about 80 lines: a `TEST` macro that registers a function in a list, and `CHECK` macros that record failures with the file and line. It was written here to avoid depending on an outside testing library.

Each test builds its own in-memory database (`":memory:"`), so tests are fast and cannot affect each other. One test uses a real file to prove that data survives closing and reopening.

## Known limits and possible next steps

- One person uses the file at a time; there is no syncing between devices.
- Weights are stored as entered. Switching between lb and kg changes the label, not the numbers.
- A practice's date and name cannot be edited after it is created (delete and recreate it).
- Ideas: a lineup builder that seats athletes by side and balances weight left to right; a per-athlete trend over time; a graphical interface on top of the same `TeamStore`.
