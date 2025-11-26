# CSV-SQL Project – Fuzz Testing with LibFuzzer

**Project type:** C mini database engine for CSV files  
**Testing focus:** Coverage-guided fuzzing with LibFuzzer

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

Because this code is heavily string-based and processes external input (CSV files and user input), it is a good candidate for robustness and security testing using fuzzing.

---

## 2. What Is Fuzz Testing?

### 2.1 Definition

**Fuzz testing** (or fuzzing) is a software testing technique where a program is executed repeatedly with automatically generated, often random or mutated, inputs. The goal is to discover:

- Crashes (segmentation faults, aborts, assertion failures).
- Memory safety issues (buffer overflows, use-after-free, double free).
- Undefined behavior and data corruption.
- Logic errors triggered by unusual or malformed inputs.

Instead of manually crafting test cases, fuzzing lets an engine generate many inputs and observe where the program behaves incorrectly or unexpectedly.

### 2.2 Types of Issues Fuzzing Can Reveal

- **Memory-safety bugs** in C:
  - Out-of-bounds reads/writes on arrays or buffers.
  - Invalid pointer dereferences.
  - Use-after-free, double free, leaks (when combined with sanitizers).
- **Parsing errors**:
  - Incorrect handling of malformed CSV lines or numeric strings.
  - Infinite loops or extremely slow behavior for certain patterns.
- **Edge cases**:
  - Empty fields, huge numbers, special characters, embedded null bytes.
  - Very long lines, maximum row/column limits, off-by-one errors.

In this project, fuzzing is especially relevant for the CSV parsing and numeric processing functions, as they are the primary interface to untrusted input.

---

## 3. What Is LibFuzzer and Why We Use It

### 3.1 LibFuzzer Overview

**LibFuzzer** is a coverage-guided fuzzing engine integrated into Clang/LLVM.  
It works by repeatedly calling a special function that we provide:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);


LibFuzzer:

Mutates the input buffer data in many ways.

Executes the target code on each mutated input.

Uses code coverage feedback to guide further mutations, trying to reach new branches and deeper states.

Works very well with sanitizers like -fsanitize=address and -fsanitize=undefined to catch memory and UB bugs.

3.2 Why We Chose LibFuzzer for This Project

Fine-grained function-level fuzzing
We can target individual functions such as parse_csv_line(), parse_double(), or check_column_unique() directly.

Good fit for C code
LibFuzzer integrates cleanly with Clang and C programs and works well with AddressSanitizer.

Coverage-guided
It automatically explores different branches in our code, which is ideal for complex parsing logic and conditional flows.

No custom fuzzing loop required
LibFuzzer provides the fuzzing loop; we only supply LLVMFuzzerTestOneInput.

3.3 Why We Chose This Program for Fuzzing

Our CSV-SQL program is a good fuzzing target because:

It handles untrusted, text-based input (CSV content and user-provided values).

The parsing and numeric conversion functions (parse_csv_line, parse_double) are error-prone if not carefully implemented.

Many functions rely on array indexing, dynamic memory, and string operations, which can easily lead to memory-safety issues in C.

The data structures (Table, Row) are reused across multiple higher-level operations, so any bug there could affect many features.

Applying fuzz testing to this project is realistic and meaningful: it simulates weird CSV files or user inputs and checks whether our code remains stable and safe.

4. Why We Focused on Coverage-Guided Fuzzing
4.1 Why Only This Testing Method

In this project we concentrated on coverage-guided fuzz testing with LibFuzzer, rather than implementing many other complex test strategies, for these reasons:

High impact on robustness – fuzzing is very effective at discovering low-level memory errors, which are the most serious bugs in C programs.

Automated generation of inputs – we do not need to manually design a huge test suite; LibFuzzer automatically mutates inputs to explore new paths.

Time and scope constraints – with a two-member team and limited time, focusing on one strong method (LibFuzzer) allowed us to reach deeper coverage instead of shallow unit tests everywhere.

Direct integration with our code – by including csv_sql.c in each fuzz target, we exercise the real production implementation rather than recreating simplified versions.

4.2 Why We Chose These Specific Functions to Fuzz

We did not fuzz every single helper function. Instead, we chose functions that:

Take external or attacker-controlled input (bytes or strings).

Perform parsing, numeric conversion, or manipulate indexes.

Have loops and conditions that may hide edge-case bugs.

Examples:

load_csv(), parse_csv_line(), and parse_double() translate arbitrary input into internal data structures.

group_by_column() and show_distinct_values() iterate through many cells.

sort_by_column() and compare_rows_by_col() rely on comparisons and indexing.

These are exactly the places where fuzzing is most likely to reveal bugs.
UI-only functions such as menu printing are much less risky from a memory-safety perspective and therefore were a lower priority.

5. Main Code Changes for Fuzzing
5.1 Conditional Compilation of main()

At the bottom of csv_sql.c we wrapped main() in a preprocessor guard:

#ifndef FUZZING
int main(void) {
    Table table;
    init_table(&table);

    /* ... interactive menu loop ... */

    free_table(&table);
    return 0;
}
#endif


Reason: LibFuzzer provides its own main() (inside the fuzzer runtime).
If csv_sql.c also compiled a main(), we would get a “multiple definition of main” linker error.

For the normal program, we compile without -DFUZZING, so main() is included.

For fuzzers, we compile with -DFUZZING, so main() is excluded and LibFuzzer’s entry point is used.

5.2 Using the Real Implementation in the Fuzzers

In each fuzz test file, we directly include the real project code:

#include "../../csv_sql.c"


Advantages:

We exercise the actual production implementation of each function.

Even static functions (like compare_rows_by_col) are visible inside the same translation unit, so we can fuzz them without changing their visibility.

5.3 Example: Fuzzer for check_column_unique()

File: test/high_priority/fuzz_check_column_unique.c

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use the REAL project code */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3) return 0;

    /* Silence output */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build table */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);
    if (t.row_count == 0) t.row_count = 1;

    /* Column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Cell values */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t usable = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t avail = size - idx;
                size_t n = usable < avail ? usable : avail;

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    /* Fake stdin: fuzz supplies column number */
    char *input = malloc(size + 1);
    memcpy(input, data, size);
    input[size] = '\0';

    FILE *fake_stdin = fmemopen(input, size, "r");

    FILE *orig_stdin = stdin;
    stdin = fake_stdin;

    /* Call REAL implementation */
    check_column_unique(&t);

    /* Restore I/O */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Free memory */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}


Key ideas:

Construct a Table entirely from fuzz data.

Redirect stdin to a memory buffer (fmemopen) so check_column_unique() can still call read_line_stdin() and get fuzz-controlled input (column index).

Redirect stdout to /dev/null to avoid clutter and improve fuzzing speed.

Free all allocated memory to avoid leaking between iterations.

5.4 Example: Fuzzer for compare_rows_by_col()

File: test/high_priority/fuzz_compare_rows_by_col.c

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import REAL implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 4) return 0;

    /* Build a Table with 2 rows */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0)
        t.col_count = 1;

    t.row_count = 2;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Build cell contents */
    for (int r = 0; r < 2; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {

                size_t usable = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t avail = size - idx;
                size_t n = (usable < avail ? usable : avail);

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    int col = data[1] % t.col_count;
    int asc = data[2] & 1;

    /* Call REAL project function */
    compare_rows_by_col(&t, &t.rows[0], &t.rows[1], col, asc);

    /* Cleanup */
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    return 0;
}


Here we fuzz row comparison logic by feeding arbitrary strings that may or may not be valid numbers, testing both numeric and lexicographic comparison paths.

6. Fuzzed Functions and Rationale

We created dedicated fuzzers for the following functions:

fuzz_check_column_unique.c → check_column_unique()

fuzz_compare_rows_by_col.c → compare_rows_by_col()

fuzz_find_rows_between.c → find_rows_between() / find_rows_in_range()

fuzz_find_rows_by_substring.c → find_rows_by_substring()

fuzz_find_rows_in_range.c → find_rows_in_range() directly

fuzz_find_rows_like.c → find_rows_like()

fuzz_group_by_column.c → group_by_column()

fuzz_load_csv.c → load_csv() and CSV parsing path

fuzz_max_by_column.c → max_by_column()

fuzz_min_by_column.c → min_by_column()

fuzz_parse_csv_line.c → parse_csv_line()

fuzz_parse_double.c → parse_double()

fuzz_show_distinct_values.c → show_distinct_values()

fuzz_sort_by_column.c → sort_by_column()

fuzz_sum_avg_column.c → sum_avg_column()

All these functions either:

Consume user-controlled data (strings, numbers, CSV lines), or

Loop and index into table rows/columns, which are typical locations for buffer/indexing bugs.

During our fuzzing sessions no crashes, sanitizer errors, or timeouts were observed for these targets under the tested conditions.
This increases our confidence in the robustness of the implementation, although fuzzing can never prove the complete absence of bugs.

7. Building and Running the Fuzzers
7.1 Prerequisites

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

7.2 Example Compilation Commands

For each fuzz target we:

Compile with LibFuzzer and AddressSanitizer.

Define FUZZING so that main() in csv_sql.c is excluded.

Example – build fuzz_check_column_unique:

clang -g -O1 \
  -fsanitize=fuzzer,address \
  -DFUZZING \
  test/high_priority/fuzz_check_column_unique.c \
  -o fuzz_check_column_unique


Example – build fuzz_compare_rows_by_col:

clang -g -O1 \
  -fsanitize=fuzzer,address \
  -DFUZZING \
  test/high_priority/fuzz_compare_rows_by_col.c \
  -o fuzz_compare_rows_by_col


Use the same pattern for all other fuzzers, changing only the source file and output name.

7.3 Running the Fuzzers

LibFuzzer binaries can be executed directly.

Basic run:

./fuzz_check_column_unique


Run for a fixed number of iterations (e.g. 100000):

./fuzz_check_column_unique -runs=100000


Using a seed corpus directory:

mkdir -p corpora/check_column_unique
./fuzz_check_column_unique corpora/check_column_unique


If a crash is found, LibFuzzer writes a test case (e.g. crash-* file) that can be replayed later.
In our fuzzing sessions on the listed targets, no such crash files were generated.

8. Conclusions from Fuzz Testing

We applied coverage-guided fuzzing using LibFuzzer to the most critical and input-heavy functions in our CSV-SQL engine.

All fuzz targets (parse_csv_line, parse_double, load_csv, aggregation functions, grouping, sorting, unique checks, etc.) ran without crashes or sanitizer violations in our experiments.

Fuzzing helped us increase confidence that:

The CSV parsing logic does not overflow buffers for the tested limits.

Numeric parsing and computations can handle many strange inputs.

Row and column indexing functions handle bounds correctly in typical fuzzed scenarios.

However, fuzz testing is not a formal proof of correctness; it reduces risk by exploring many rare edge cases that manual testing would likely miss.

9. Team Contributions

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

10. Use of AI Tools

During this project we used the AI tool ChatGPT (by OpenAI) to:

Get explanations about fuzz testing and LibFuzzer concepts.

Help structure this README/report and refine the wording.

Cross-check example compilation commands and best practices for fuzz harness design.

All final code, fuzz harnesses, and decisions were reviewed and verified by the project members before inclusion in the repository.
