# CSV-SQL Project – Fuzz Testing with LibFuzzer

**Project type:** C mini database engine for CSV files  
**Testing focus:** fuzz testing using LibFuzzer

---

## 1. Overview of the Project

This project implements a mini SQL-like engine in C that operates directly on CSV files.  
The main program is `csv_sql.c`, which provides a menu-driven interface for loading, querying, and modifying tabular data stored in CSV format.

### 1.1 Core Features of `csv_sql.c`

- Load a CSV file into an in-memory `Table` structure (`load_csv()`).
- Show CSV summary (row count, column count, header).
- View first / last `N` rows.
- Insert, delete, and update a single row.
- Search operations:
  - Exact match: `find_rows_by_value()`
  - Substring / LIKE search: `find_rows_like()`
  - Numeric BETWEEN query: `find_rows_between()`
- Aggregate operations:
  - MAX / MIN by column: `max_by_column()`, `min_by_column()`
  - SUM and AVG on numeric column: `sum_avg_column()`
- Data quality checks:
  - Check duplicates in a column: `check_column_unique()`
  - DISTINCT values: `show_distinct_values()`
  - GROUP BY column: `group_by_column()`
- Sorting:
  - Ascending / descending by column: `sort_by_column()`
  - Row comparison helper: `compare_rows_by_col()`
- Parsing and numeric helpers:
  - `parse_csv_line()` – split a CSV line into fields.
  - `parse_double()` – robust conversion from string to `double`.
- Saving the modified table back to a CSV file: `save_csv()`.

Because this code is heavily string-based and processes external input (CSV files and user input), it is a good project for robustness and security testing using fuzzing.

---

## 2. What Is Fuzz Testing?

### 2.1 Definition

**Fuzz testing** (or fuzzing) is a software testing technique where a program is executed repeatedly with automatically generated, often random or mutated, inputs. The goal is to discover:

- Crashes (segmentation faults).
- Memory safety issues (buffer overflows).
- Undefined behavior and data corruption.
- Logic errors triggered by unusual or malformed inputs.

Instead of manually crafting test cases, fuzzing lets an engine generate many inputs and observe where the program behaves incorrectly or unexpectedly.

### 2.2 Types of Issues Fuzzing Can Reveal

- **Memory-safety bugs** in C:
  - Out-of-bounds reads/writes on arrays or buffers.
  - Invalid pointer dereferences.
  - Use-after-free, leaks.
- **Parsing errors**:
  - Incorrect handling of malformed CSV lines or numeric strings.
  - Infinite loops or extremely slow behavior for certain patterns.
- **Edge cases**:
  - Empty fields, huge numbers, special characters, embedded null bytes.
  - Very long lines, maximum row/column limits.

In this project, fuzzing is especially relevant for the CSV parsing and numeric processing functions, as they are the primary interface to untrusted input.

---

## 3. What Is LibFuzzer and Why We Use It

### 3.1 LibFuzzer Overview

- **LibFuzzer** is a fuzzing engine that works inside your program (“in-process”), and it is coverage-guided.
- That means it doesn’t just generate random data blindly — it uses feedback about which parts of your code have been reached (coverage data) to guide further testing. 
- It works especially well with C/C++ code.

### 3.2 Why We Chose LibFuzzer for This Project

- **Fine-grained function-level fuzzing**
  - We can test individual functions directly, like `parse_csv_line()`, `parse_double()`, or `check_column_unique()`.
  - This helps us focus on the most important and risky parts of the code.

- **Works very well with C programs**
  - LibFuzzer is built into Clang/LLVM, so it fits naturally with C projects.
  - It also works smoothly with tools like AddressSanitizer, which helps catch memory errors.

- **Uses code coverage to improve testing**
  - LibFuzzer automatically looks for new paths and branches in the code.
  - This makes it great for testing complicated parsing logic and conditions.

- **No need to write our own fuzzing loop**
  - LibFuzzer already has the fuzzing loop built in.
  - We only need to write the `LLVMFuzzerTestOneInput()` function to feed data into our code.

### 3.3 Why We Chose This Program for Fuzzing

- It handles untrusted, text-based input (CSV content and user-provided values).
- The parsing and numeric conversion functions (parse_csv_line, parse_double) are error-prone if not carefully implemented.
- Many functions rely on array indexing, dynamic memory, and string operations, which can easily lead to memory-safety issues in C.
- The data structures (Table, Row) are reused across multiple higher-level operations, so any bug there could affect many features.
- Applying fuzz testing to this project is realistic and meaningful: it simulates weird CSV files or user inputs and checks whether our code remains stable and safe.

---

## 4. Why We Focused on Coverage-Guided Fuzzing
### 4.1 Why Only This Testing Method

- High impact on robustness – fuzzing is very effective at discovering low-level memory errors, which are the most serious bugs in C programs.

- Automated generation of inputs – we do not need to manually design a huge test suite; LibFuzzer automatically mutates inputs to explore new paths.

- Direct integration with our code – by including csv_sql.c in each fuzz target, we exercise the real production implementation rather than recreating simplified versions.

### 4.2 Why We Chose These Specific Functions to Fuzz

We did not fuzz every single helper function. Instead, we chose functions that:

- Take external or attacker-controlled input (bytes or strings).

- Perform parsing, numeric conversion, or manipulate indexes.

- Have loops and conditions that may hide edge-case bugs.

- Examples:

   - load_csv(), parse_csv_line(), and parse_double() translate arbitrary input into internal data structures.

   - group_by_column() and show_distinct_values() iterate through many cells.

   - sort_by_column() and compare_rows_by_col() rely on comparisons and indexing.

- These are exactly the places where fuzzing is most likely to reveal bugs.
- UI-only functions such as menu printing are much less risky from a memory-safety perspective and therefore were a lower priority.

---

## 5. Main Code Changes for Fuzzing
### 5.1 Conditional Compilation of main()

At the bottom of csv_sql.c we wrapped main() in a preprocessor guard:

#### #ifndef FUZZING
#### int main(void) {
#### .....
#### }
#### #endif

Reason: LibFuzzer provides its own main() (inside the fuzzer runtime).
If csv_sql.c also compiled a main(), we would get a “multiple definition of main” linker error.
For the normal program, we compile without -DFUZZING, so main() is included.
For fuzzers, we compile with -DFUZZING, so main() is excluded and LibFuzzer’s entry point is used.

### 5.2 Using the Real Implementation in the Fuzzers

In each fuzz test file, we directly include the real project code:
#include "../../csv_sql.c"

Advantages:
We exercise the actual production implementation of each function.

---

## 6. Fuzzed Functions and Rationale

We created dedicated fuzzers for the following functions:

- fuzz_check_column_unique.c → check_column_unique()
- fuzz_compare_rows_by_col.c → compare_rows_by_col()
- fuzz_find_rows_between.c → find_rows_between() / find_rows_in_range()
- fuzz_find_rows_by_substring.c → find_rows_by_substring()
- fuzz_find_rows_in_range.c → find_rows_in_range() directly
- fuzz_find_rows_like.c → find_rows_like()
- fuzz_group_by_column.c → group_by_column()
- fuzz_load_csv.c → load_csv() and CSV parsing path
- fuzz_max_by_column.c → max_by_column()
- fuzz_min_by_column.c → min_by_column()
- fuzz_parse_csv_line.c → parse_csv_line()
- fuzz_parse_double.c → parse_double()
- fuzz_show_distinct_values.c → show_distinct_values()
- fuzz_sort_by_column.c → sort_by_column()
- fuzz_sum_avg_column.c → sum_avg_column()

All these functions either:
- Consume user-controlled data (strings, numbers, CSV lines), or
- Loop and index into table rows/columns, which are typical locations for buffer/indexing bugs.
  
---

## 7. Building and Running the Fuzzers
### 7.1 Prerequisites

Clang/LLVM with LibFuzzer support (typically clang on Linux).

Recommended sanitizers: -fsanitize=fuzzer,address
(or -fsanitize=fuzzer,address,undefined).

Project structure (example):

project_root/
  csv_sql.c
  test/
    high_priority/
      fuzz_check_column_unique.c
      fuzz_compare_rows_by_col.c
      ...

### 7.2 Example Compilation Commands

For each fuzz target we:

Compile with LibFuzzer and AddressSanitizer.

Define FUZZING so that main() in csv_sql.c is excluded.

Example – build fuzz_check_column_unique:

clang -g -O1 \
  -fsanitize=fuzzer,address \
  -DFUZZING \
  test/high_priority/fuzz_check_column_unique.c \
  -o fuzz_check_column_unique

Use the same pattern for all other fuzzers, changing only the source file and output name.

### 7.3 Running the Fuzzers

LibFuzzer binaries can be executed directly.

Basic run:

./fuzz_check_column_unique

Run for a fixed number of iterations (e.g. 100000):

./fuzz_check_column_unique -runs=100000

If a crash is found, LibFuzzer writes a test case (e.g. crash-* file) that can be replayed later.

---

## 8. Conclusions from Fuzz Testing

- We applied coverage-guided fuzzing using LibFuzzer to the most critical and input-heavy functions in our CSV-SQL engine.

- All fuzz targets (parse_csv_line, parse_double, load_csv, aggregation functions, grouping, sorting, unique checks, etc.) ran without crashes or sanitizer violations in our experiments.

- Fuzzing helped us increase confidence that:

    - The CSV parsing logic does not overflow buffers for the tested limits.

    - Numeric parsing and computations can handle many strange inputs.

    - Row and column indexing functions handle bounds correctly in typical fuzzed scenarios.

- However, fuzz testing is not a formal proof of correctness; it reduces risk by exploring many rare edge cases that manual testing would likely miss.

---

## 9. Team Contributions

This project was completed by a two-member team.

Member 1

Implemented and refactored the main csv_sql.c functionality:
loading CSV, viewing rows, aggregates, sorting, GROUP BY, DISTINCT, and duplicate checking.

Added conditional compilation around main() using #ifndef FUZZING to make the code compatible with LibFuzzer.

Helped design the overall testing strategy and selected which functions should be fuzzed.

Reviewed and executed fuzzing runs and checked for crashes or sanitizer reports.

Member 2

Wrote the individual fuzz harnesses:
fuzz_check_column_unique.c,
fuzz_compare_rows_by_col.c, and the other fuzz_*.c files.

Implemented the table-building logic inside fuzzers (allocating cells, mapping bytes to strings).

Configured the use of fake stdin and redirecting stdout to /dev/null to integrate with interactive functions.

Managed compilation commands and organized high-priority fuzz targets.

Both members collaborated on interpreting fuzzing results, cleaning up code, and preparing this report.

---

## 10. Use of AI Tools

- During this project we used the AI tool ChatGPT (by OpenAI).

