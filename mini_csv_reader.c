/*
 * mini_csv_reader.c
 *
 * A small but feature-rich CSV file reader in C.
 *
 * Features:
 *  - Simple CSV parser with:
 *      * custom delimiter (default: ',')
 *      * quoted fields with "
 *      * support for delimiter inside quotes
 *  - Loads CSV into memory (dynamic resizing).
 *  - Supports header row.
 *  - Menu-based CLI:
 *      1. Load CSV file
 *      2. Show basic summary (rows, columns, header)
 *      3. View a subset of rows
 *      4. Filter rows by exact text match in a column
 *      5. Filter rows by substring in a column
 *      6. Numeric column operations (min/max/sum/avg)
 *      7. Save filtered result to new CSV
 *      8. Change delimiter / header settings
 *      9. Exit
 *
 * This is suitable as a project for fuzz testing:
 *  - You can fuzz parse_csv_line()
 *  - You can fuzz numeric parsing on columns
 *
 * Compile:
 *      gcc -Wall -Wextra -O2 mini_csv_reader.c -o csv_reader
 *
 * Run:
 *      ./csv_reader
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ========================= Utility Macros ========================= */

#define MAX_LINE_LEN 4096
#define INITIAL_ROW_CAP 128
#define INITIAL_COL_CAP 32
#define INPUT_BUF_SIZE 256

/* Macro to free pointer and set to NULL */
#define SAFE_FREE(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)

/* ========================= Data Structures ========================= */

typedef struct {
    char **cells;      /* array of strings representing fields */
    size_t cell_count; /* number of used cells */
    size_t capacity;   /* allocated capacity */
} CsvRow;

typedef struct {
    CsvRow *rows;      /* array of rows */
    size_t row_count;  /* actual number of rows */
    size_t capacity;   /* allocated number of rows */
    int has_header;    /* 1 if row 0 is header */
} CsvTable;

typedef struct {
    char delimiter;    /* e.g., ',' or ';' or '\t' */
    int has_header;    /* whether file has header row */
} CsvSettings;

/* Global settings for simplicity */
static CsvSettings g_settings = { ',', 1 };

/* ========================= Utility Functions ========================= */

/* Trim trailing newline from fgets buffer */
static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

/* Read a line of input from stdin (for menu), safely */
static void read_input_line(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

/* strdup replacement (to avoid non-standard strdup warning) */
static char *str_dup(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *p = (char *)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, src, len + 1);
    return p;
}

/* Check if string represents a numeric value (int/float-ish) */
static int is_numeric_string(const char *s) {
    if (!s || *s == '\0') return 0;
    int dot_count = 0;
    const char *p = s;

    /* Optional leading sign */
    if (*p == '-' || *p == '+') {
        p++;
        if (*p == '\0') return 0;
    }

    for (; *p; p++) {
        if (*p == '.') {
            dot_count++;
            if (dot_count > 1) return 0;
        } else if (!isdigit((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

/* ========================= CsvRow Functions ========================= */

/* Initialize a CsvRow */
static void csv_row_init(CsvRow *row) {
    if (!row) return;
    row->cells = NULL;
    row->cell_count = 0;
    row->capacity = 0;
}

/* Free memory for a CsvRow */
static void csv_row_free(CsvRow *row) {
    if (!row) return;
    for (size_t i = 0; i < row->cell_count; i++) {
        SAFE_FREE(row->cells[i]);
    }
    SAFE_FREE(row->cells);
    row->cell_count = 0;
    row->capacity = 0;
}

/* Ensure capacity for at least new_capacity cells */
static int csv_row_reserve(CsvRow *row, size_t new_capacity) {
    if (!row) return 0;
    if (new_capacity <= row->capacity) return 1;

    size_t new_cap = row->capacity ? row->capacity * 2 : INITIAL_COL_CAP;
    if (new_cap < new_capacity) new_cap = new_capacity;

    char **new_cells = (char **)realloc(row->cells, new_cap * sizeof(char *));
    if (!new_cells) {
        fprintf(stderr, "Error: Out of memory while expanding row cells\n");
        return 0;
    }
    row->cells = new_cells;
    /* Initialize new cells to NULL */
    for (size_t i = row->capacity; i < new_cap; i++) {
        row->cells[i] = NULL;
    }
    row->capacity = new_cap;
    return 1;
}

/* Push a cell string to row (takes ownership of heap-allocated string) */
static int csv_row_push_cell(CsvRow *row, char *cell_value) {
    if (!row) return 0;
    if (row->cell_count == row->capacity) {
        if (!csv_row_reserve(row, row->cell_count + 1)) {
            SAFE_FREE(cell_value);
            return 0;
        }
    }
    row->cells[row->cell_count++] = cell_value;
    return 1;
}

/* Print a CsvRow */
static void csv_row_print(const CsvRow *row) {
    if (!row) return;
    for (size_t i = 0; i < row->cell_count; i++) {
        printf("%s", row->cells[i] ? row->cells[i] : "");
        if (i + 1 < row->cell_count) {
            printf(" | ");
        }
    }
    printf("\n");
}

/* ========================= CsvTable Functions ========================= */

/* Initialize table */
static void csv_table_init(CsvTable *table) {
    if (!table) return;
    table->rows = NULL;
    table->row_count = 0;
    table->capacity = 0;
    table->has_header = 0;
}

/* Free entire table */
static void csv_table_free(CsvTable *table) {
    if (!table) return;
    for (size_t i = 0; i < table->row_count; i++) {
        csv_row_free(&table->rows[i]);
    }
    SAFE_FREE(table->rows);
    table->row_count = 0;
    table->capacity = 0;
}

/* Ensure capacity for rows */
static int csv_table_reserve(CsvTable *table, size_t new_capacity) {
    if (!table) return 0;
    if (new_capacity <= table->capacity) return 1;

    size_t new_cap = table->capacity ? table->capacity * 2 : INITIAL_ROW_CAP;
    if (new_cap < new_capacity) new_cap = new_capacity;

    CsvRow *new_rows = (CsvRow *)realloc(table->rows, new_cap * sizeof(CsvRow));
    if (!new_rows) {
        fprintf(stderr, "Error: Out of memory while expanding table rows\n");
        return 0;
    }

    table->rows = new_rows;
    /* Initialize new rows */
    for (size_t i = table->capacity; i < new_cap; i++) {
        csv_row_init(&table->rows[i]);
    }
    table->capacity = new_cap;
    return 1;
}

/* Add a row to the table (copy contents from provided row) */
static int csv_table_add_row(CsvTable *table, const CsvRow *source) {
    if (!table || !source) return 0;
    if (table->row_count == table->capacity) {
        if (!csv_table_reserve(table, table->row_count + 1)) {
            return 0;
        }
    }

    CsvRow *target = &table->rows[table->row_count];
    csv_row_init(target);

    for (size_t i = 0; i < source->cell_count; i++) {
        char *dup = str_dup(source->cells[i] ? source->cells[i] : "");
        if (!dup) {
            fprintf(stderr, "Error: Out of memory while copying row\n");
            csv_row_free(target);
            return 0;
        }
        if (!csv_row_push_cell(target, dup)) {
            csv_row_free(target);
            return 0;
        }
    }

    table->row_count++;
    return 1;
}

/* Get column count (based on first row) */
static size_t csv_table_column_count(const CsvTable *table) {
    if (!table || table->row_count == 0) return 0;
    return table->rows[0].cell_count;
}

/* Print a brief summary of the table */
static void csv_table_print_summary(const CsvTable *table) {
    if (!table) {
        printf("No table loaded.\n");
        return;
    }
    printf("\n=== CSV Summary ===\n");
    printf("Rows:   %zu\n", table->row_count);
    printf("Cols:   %zu\n", csv_table_column_count(table));
    printf("Header: %s\n", table->has_header ? "Yes" : "No");

    if (table->has_header && table->row_count > 0) {
        printf("Header row:\n");
        csv_row_print(&table->rows[0]);
    }
    printf("===================\n");
}

/* Print first N rows (or all if fewer) */
static void csv_table_print_rows(const CsvTable *table, size_t max_rows) {
    if (!table || table->row_count == 0) {
        printf("No data to display.\n");
        return;
    }

    size_t limit = (max_rows > 0 && max_rows < table->row_count) ?
                   max_rows : table->row_count;

    for (size_t i = 0; i < limit; i++) {
        printf("%5zu: ", i);
        csv_row_print(&table->rows[i]);
    }
}

/* ========================= CSV Parsing ========================= */

/*
 * parse_csv_line
 *
 * Parses a single CSV line into fields, handling:
 *  - delimiter (configurable)
 *  - quoted fields with "
 *  - delimiter inside quotes
 *  - escaped quotes inside quoted fields via ""
 *
 * Input:
 *  - line: string to parse (will NOT be modified)
 *  - delimiter: e.g. ','
 *  - out_row: CsvRow to fill (must be initialized)
 *
 * Returns:
 *  - 1 on success
 *  - 0 on failure
 */
static int parse_csv_line(const char *line, char delimiter, CsvRow *out_row) {
    if (!line || !out_row) return 0;

    csv_row_free(out_row);
    csv_row_init(out_row);

    const char *p = line;
    size_t len = strlen(line);
    char *field = (char *)malloc(len + 1); /* worst case: whole line is one field */
    if (!field) {
        fprintf(stderr, "Error: Out of memory in parse_csv_line\n");
        return 0;
    }

    size_t field_len = 0;
    int in_quotes = 0;

    while (*p) {
        char c = *p;

        if (in_quotes) {
            if (c == '"') {
                /* Check for escaped double-quote ("") */
                if (*(p + 1) == '"') {
                    field[field_len++] = '"';
                    p++; /* skip second quote */
                } else {
                    in_quotes = 0;
                }
            } else {
                field[field_len++] = c;
            }
        } else {
            if (c == '"') {
                in_quotes = 1;
            } else if (c == delimiter) {
                /* end of field */
                field[field_len] = '\0';
                char *cell_value = str_dup(field);
                if (!cell_value || !csv_row_push_cell(out_row, cell_value)) {
                    SAFE_FREE(field);
                    return 0;
                }
                field_len = 0;
            } else if (c == '\r' || c == '\n') {
                /* End of line */
                break;
            } else {
                field[field_len++] = c;
            }
        }
        p++;
    }

    /* Final field */
    field[field_len] = '\0';
    char *cell_value = str_dup(field);
    SAFE_FREE(field);
    if (!cell_value || !csv_row_push_cell(out_row, cell_value)) {
        return 0;
    }

    return 1;
}

/*
 * Load an entire CSV file into a CsvTable.
 * Applies global settings: g_settings.delimiter and g_settings.has_header.
 */
static int csv_table_load_from_file(const char *filename, CsvTable *table) {
    if (!filename || !table) return 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening CSV file");
        return 0;
    }

    csv_table_free(table);
    csv_table_init(table);
    table->has_header = g_settings.has_header;

    char buf[MAX_LINE_LEN];

    while (fgets(buf, sizeof(buf), f) != NULL) {
        trim_newline(buf);
        if (buf[0] == '\0') {
            /* Skip empty lines */
            continue;
        }
        CsvRow row;
        csv_row_init(&row);
        if (!parse_csv_line(buf, g_settings.delimiter, &row)) {
            fprintf(stderr, "Error parsing line: %s\n", buf);
            csv_row_free(&row);
            fclose(f);
            return 0;
        }
        if (!csv_table_add_row(table, &row)) {
            csv_row_free(&row);
            fclose(f);
            return 0;
        }
        csv_row_free(&row);
    }

    fclose(f);
    return 1;
}

/* Save a CsvTable to file in CSV format with given delimiter */
static int csv_table_save_to_file(const char *filename, const CsvTable *table, char delimiter) {
    if (!filename || !table) return 0;

    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error opening output CSV file");
        return 0;
    }

    for (size_t i = 0; i < table->row_count; i++) {
        const CsvRow *row = &table->rows[i];
        for (size_t j = 0; j < row->cell_count; j++) {
            const char *cell = row->cells[j] ? row->cells[j] : "";
            int needs_quotes = 0;

            /* Check if cell contains delimiter, quote, newline, or spaces */
            for (const char *p = cell; *p; p++) {
                if (*p == delimiter || *p == '"' || *p == '\n' || *p == '\r' || *p == ' ') {
                    needs_quotes = 1;
                    break;
                }
            }

            if (needs_quotes) {
                fputc('"', f);
                for (const char *p = cell; *p; p++) {
                    if (*p == '"') {
                        fputc('"', f); /* escape quotes */
                    }
                    fputc(*p, f);
                }
                fputc('"', f);
            } else {
                fputs(cell, f);
            }

            if (j + 1 < row->cell_count) {
                fputc(delimiter, f);
            }
        }
        fputc('\n', f);
    }

    fclose(f);
    return 1;
}

/* ========================= Filtering & Numeric Ops ========================= */

/*
 * Filter by exact match on a column.
 * Returns a new CsvTable containing matching rows.
 * Includes header if original had header.
 */
static int csv_table_filter_exact(const CsvTable *src, size_t column_index,
                                  const char *match_value, CsvTable *out) {
    if (!src || !match_value || !out) return 0;

    csv_table_free(out);
    csv_table_init(out);
    out->has_header = src->has_header;

    size_t start_idx = 0;
    if (src->has_header && src->row_count > 0) {
        /* Copy header row as-is */
        if (!csv_table_add_row(out, &src->rows[0])) return 0;
        start_idx = 1;
    }

    for (size_t i = start_idx; i < src->row_count; i++) {
        const CsvRow *row = &src->rows[i];
        if (column_index >= row->cell_count) {
            /* skip if row too short */
            continue;
        }
        const char *cell = row->cells[column_index] ? row->cells[column_index] : "";
        if (strcmp(cell, match_value) == 0) {
            if (!csv_table_add_row(out, row)) return 0;
        }
    }

    return 1;
}

/*
 * Filter by substring match on a column.
 */
static int csv_table_filter_substring(const CsvTable *src, size_t column_index,
                                      const char *substr, CsvTable *out) {
    if (!src || !substr || !out) return 0;

    csv_table_free(out);
    csv_table_init(out);
    out->has_header = src->has_header;

    size_t start_idx = 0;
    if (src->has_header && src->row_count > 0) {
        if (!csv_table_add_row(out, &src->rows[0])) return 0;
        start_idx = 1;
    }

    for (size_t i = start_idx; i < src->row_count; i++) {
        const CsvRow *row = &src->rows[i];
        if (column_index >= row->cell_count) continue;
        const char *cell = row->cells[column_index] ? row->cells[column_index] : "";
        if (strstr(cell, substr) != NULL) {
            if (!csv_table_add_row(out, row)) return 0;
        }
    }

    return 1;
}

/*
 * Compute numeric statistics for a given column:
 *  - count of numeric cells
 *  - min
 *  - max
 *  - sum
 *  - average
 *
 * Only cells that pass is_numeric_string() are used.
 */
static int csv_table_numeric_stats(const CsvTable *table, size_t column_index) {
    if (!table) return 0;
    if (table->row_count == 0) {
        printf("Table is empty.\n");
        return 0;
    }

    double min_val = 0.0;
    double max_val = 0.0;
    double sum_val = 0.0;
    size_t count = 0;

    size_t start_idx = table->has_header ? 1 : 0;

    for (size_t i = start_idx; i < table->row_count; i++) {
        const CsvRow *row = &table->rows[i];
        if (column_index >= row->cell_count) continue;

        const char *cell = row->cells[column_index] ? row->cells[column_index] : "";
        if (!is_numeric_string(cell)) {
            continue;
        }

        double val = atof(cell);
        if (count == 0) {
            min_val = max_val = val;
        } else {
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        sum_val += val;
        count++;
    }

    if (count == 0) {
        printf("No numeric values found in column %zu.\n", column_index);
        return 0;
    }

    double avg = sum_val / count;

    printf("\n=== Numeric Stats for Column %zu ===\n", column_index);
    printf("Count: %zu\n", count);
    printf("Min:   %.6f\n", min_val);
    printf("Max:   %.6f\n", max_val);
    printf("Sum:   %.6f\n", sum_val);
    printf("Avg:   %.6f\n", avg);
    printf("====================================\n");

    return 1;
}

/* ========================= Settings & Menu Helpers ========================= */

static void print_settings(void) {
    printf("\n=== Current CSV Settings ===\n");
    printf("Delimiter: '%c'\n", g_settings.delimiter == '\t' ? 't' : g_settings.delimiter);
    printf("Has header: %s\n", g_settings.has_header ? "Yes" : "No");
    printf("============================\n\n");
}

static void change_settings(void) {
    print_settings();
    char buf[INPUT_BUF_SIZE];

    printf("Enter new delimiter (single char, 't' for tab) or blank to keep: ");
    read_input_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        if (buf[0] == 't') {
            g_settings.delimiter = '\t';
        } else {
            g_settings.delimiter = buf[0];
        }
    }

    printf("Does the CSV have header row? (1 = yes, 0 = no, blank to keep): ");
    read_input_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        int v = atoi(buf);
        g_settings.has_header = (v != 0);
    }

    print_settings();
}

/* Ask user for a size_t index (column or row index) */
static size_t ask_for_index(const char *prompt) {
    char buf[INPUT_BUF_SIZE];
    printf("%s", prompt);
    read_input_line(buf, sizeof(buf));
    return (size_t)strtoul(buf, NULL, 10);
}

/* Ask for file name */
static void ask_for_filename(const char *prompt, char *out, size_t out_size) {
    printf("%s", prompt);
    read_input_line(out, out_size);
}
/* ============================================================
 *  Display SELECT-style view (MySQL-like table)
 *  cols[] = list of column indexes to display
 *  col_count = number of columns in cols[]
 *  limit = max rows to show (0 = all)
 * ============================================================ */
static void csv_table_select_mysql(const CsvTable *table,
                                   const size_t *cols,
                                   size_t col_count,
                                   size_t limit)
{
    if (!table || table->row_count == 0) {
        printf("No data loaded.\n");
        return;
    }

    size_t total_rows = table->row_count;
    if (limit > 0 && limit < total_rows) {
        total_rows = limit;
    }

    /* ------ Determine max width for each selected column ----- */
    size_t *widths = calloc(col_count, sizeof(size_t));
    if (!widths) {
        printf("Out of memory.\n");
        return;
    }

    for (size_t i = 0; i < col_count; i++) {
        size_t col = cols[i];
        size_t maxw = 3; /* min width */

        for (size_t r = 0; r < total_rows; r++) {
            const CsvRow *row = &table->rows[r];
            const char *cell = (col < row->cell_count && row->cells[col])
                               ? row->cells[col] : "";
            size_t len = strlen(cell);
            if (len > maxw) maxw = len;
        }
        widths[i] = maxw;
    }

    /* ------ Helper to print a horizontal border ------ */
    printf("\n");
    printf("+");
    for (size_t i = 0; i < col_count; i++) {
        for (size_t j = 0; j < widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n");

    /* ------ Print header row ------ */
    printf("|");
    for (size_t i = 0; i < col_count; i++) {
        printf(" %-*zu |", (int)widths[i], cols[i]);
    }
    printf("\n");

    /* ------ Print border under header ------ */
    printf("+");
    for (size_t i = 0; i < col_count; i++) {
        for (size_t j = 0; j < widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n");

    /* ------ Print data rows ------ */
    for (size_t r = 0; r < total_rows; r++) {
        const CsvRow *row = &table->rows[r];
        printf("|");
        for (size_t i = 0; i < col_count; i++) {
            size_t col = cols[i];
            const char *cell = (col < row->cell_count && row->cells[col])
                               ? row->cells[col] : "";
            printf(" %-*s |", (int)widths[i], cell);
        }
        printf("\n");
    }

    /* ------ Lower border ------ */
    printf("+");
    for (size_t i = 0; i < col_count; i++) {
        for (size_t j = 0; j < widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n\n");

    free(widths);
}


/* ========================= MAIN MENU ========================= */

static void print_main_menu(void) {
    printf("\n============== MINI CSV READER ==============\n");
    printf("1. Load CSV file\n");
    printf("2. Show CSV summary\n");
    printf("3. View first N rows\n");
    printf("4. Filter rows by exact text match (new table)\n");
    printf("5. Filter rows by substring match (new table)\n");
    printf("6. Numeric stats on column (min/max/sum/avg)\n");
    printf("7. Save current table to CSV\n");
    printf("8. Change settings (delimiter/header)\n");
    printf("9. Exit\n");
    printf("10. SELECT-style view (MySQL format)\n");
    printf("=============================================\n");
    printf("Enter choice: ");
}

/* ========================= MAIN ========================= */

int main(void) {
    CsvTable table;
    csv_table_init(&table);

    char input[INPUT_BUF_SIZE];
    int running = 1;

    printf("Welcome to Mini CSV Reader!\n");
    print_settings();

    while (running) {
        print_main_menu();
        read_input_line(input, sizeof(input));
        int choice = atoi(input);

        switch (choice) {
            case 1: {
                char filename[INPUT_BUF_SIZE];
                ask_for_filename("Enter CSV filename to load: ", filename, sizeof(filename));
                if (strlen(filename) == 0) {
                    printf("No filename entered.\n");
                    break;
                }
                if (csv_table_load_from_file(filename, &table)) {
                    printf("Successfully loaded CSV file '%s'.\n", filename);
                    table.has_header = g_settings.has_header;
                } else {
                    printf("Failed to load CSV file '%s'.\n", filename);
                }
                break;
            }

            case 2: {
                csv_table_print_summary(&table);
                break;
            }

            case 3: {
                size_t n = ask_for_index("Enter number of rows to view: ");
                if (n == 0) n = 10;
                csv_table_print_rows(&table, n);
                break;
            }

            case 4: {
                if (table.row_count == 0) {
                    printf("Load a CSV first.\n");
                    break;
                }
                size_t col_idx = ask_for_index("Column index for exact match filter: ");
                printf("Enter value to match exactly: ");
                char value[INPUT_BUF_SIZE];
                read_input_line(value, sizeof(value));
                if (strlen(value) == 0) {
                    printf("Empty match value.\n");
                    break;
                }
                CsvTable filtered;
                csv_table_init(&filtered);
                if (csv_table_filter_exact(&table, col_idx, value, &filtered)) {
                    csv_table_free(&table);
                    table = filtered;
                    printf("Filter applied. Current table now has %zu rows.\n", table.row_count);
                } else {
                    csv_table_free(&filtered);
                    printf("Error while filtering.\n");
                }
                break;
            }

            case 5: {
                if (table.row_count == 0) {
                    printf("Load a CSV first.\n");
                    break;
                }
                size_t col_idx = ask_for_index("Column index for substring match filter: ");
                printf("Enter substring to search: ");
                char substr[INPUT_BUF_SIZE];
                read_input_line(substr, sizeof(substr));
                if (strlen(substr) == 0) {
                    printf("Empty substring.\n");
                    break;
                }
                CsvTable filtered;
                csv_table_init(&filtered);
                if (csv_table_filter_substring(&table, col_idx, substr, &filtered)) {
                    csv_table_free(&table);
                    table = filtered;
                    printf("Filter applied. Current table now has %zu rows.\n", table.row_count);
                } else {
                    csv_table_free(&filtered);
                    printf("Error while filtering.\n");
                }
                break;
            }

            case 6: {
                if (table.row_count == 0) {
                    printf("Load a CSV first.\n");
                    break;
                }
                size_t col_idx = ask_for_index("Column index for numeric stats: ");
                csv_table_numeric_stats(&table, col_idx);
                break;
            }

            case 7: {
                if (table.row_count == 0) {
                    printf("No data to save.\n");
                    break;
                }
                char outname[INPUT_BUF_SIZE];
                ask_for_filename("Enter output CSV filename: ", outname, sizeof(outname));
                if (strlen(outname) == 0) {
                    printf("No filename.\n");
                    break;
                }
                if (csv_table_save_to_file(outname, &table, g_settings.delimiter)) {
                    printf("Saved current table to '%s'.\n", outname);
                } else {
                    printf("Error saving table.\n");
                }
                break;
            }

            case 8: {
                change_settings();
                break;
            }

            case 9: {
                printf("Exiting Mini CSV Reader.\n");
                running = 0;
                break;
            }
            case 10: {
                if (table.row_count == 0) {
                printf("Load a CSV first.\n");
                break;
                }

                printf("Enter column indexes separated by spaces (example: 0 2 4):\n> ");
                char buf[256];
                read_input_line(buf, sizeof(buf));

                size_t cols[32];
                size_t col_count = 0;

                char *tok = strtok(buf, " ");
                while (tok && col_count < 32) {
                cols[col_count++] = strtoul(tok, NULL, 10);
                tok = strtok(NULL, " ");
                }

                if (col_count == 0) {
                printf("No columns chosen.\n");
                break;
                }

                size_t limit = ask_for_index("Enter row limit (0 = all): ");
                csv_table_select_mysql(&table, cols, col_count, limit);
                break;
            }

            default:
                printf("Invalid choice, try again.\n");
                break;
        }
    }

    csv_table_free(&table);
    return 0;
}

