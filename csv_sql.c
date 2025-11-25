#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ROWS        1024
#define MAX_COLS        16
#define MAX_FIELD_LEN   128
#define MAX_LINE_LEN    1024

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

typedef struct {
    char *col_names[MAX_COLS];
    int col_count;
    Row rows[MAX_ROWS];
    int row_count;
} Table;

static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static void read_line_stdin(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);
    return p;
}

static int parse_double(const char *s, double *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static void init_row(Row *r) {
    if (!r) return;
    r->cell_count = 0;
    for (int i = 0; i < MAX_COLS; i++) {
        r->cells[i] = NULL;
    }
}

static void free_row(Row *r) {
    if (!r) return;
    for (int i = 0; i < r->cell_count; i++) {
        free(r->cells[i]);
        r->cells[i] = NULL;
    }
    r->cell_count = 0;
}

static void init_table(Table *t) {
    if (!t) return;
    t->col_count = 0;
    t->row_count = 0;
    for (int i = 0; i < MAX_COLS; i++) {
        t->col_names[i] = NULL;
    }
    for (int i = 0; i < MAX_ROWS; i++) {
        init_row(&t->rows[i]);
    }
}

static void free_table(Table *t) {
    if (!t) return;
    for (int i = 0; i < t->col_count; i++) {
        free(t->col_names[i]);
        t->col_names[i] = NULL;
    }
    for (int i = 0; i < t->row_count; i++) {
        free_row(&t->rows[i]);
    }
    t->col_count = 0;
    t->row_count = 0;
}

static void print_row(const Table *t, const Row *r) {
    if (!t || !r) return;
    for (int i = 0; i < t->col_count; i++) {
        const char *val = (i < r->cell_count && r->cells[i]) ? r->cells[i] : "NULL";
        printf("%s", val);
        if (i + 1 < t->col_count) printf(" | ");
    }
    printf("\n");
}

static void print_header(const Table *t) {
    if (!t) return;
    for (int i = 0; i < t->col_count; i++) {
        printf("%s", t->col_names[i] ? t->col_names[i] : "(col)");
        if (i + 1 < t->col_count) printf(" | ");
    }
    printf("\n");
}

int parse_csv_line(const char *line, char *fields[], int max_fields) {
    if (!line || !fields || max_fields <= 0) return 0;

    char buffer[MAX_LINE_LEN];
    size_t len = strlen(line);
    if (len >= sizeof(buffer)) {
        return 0;
    }
    strcpy(buffer, line);

    int count = 0;
    char *p = buffer;
    while (count < max_fields) {
        char *comma = strchr(p, ',');
        if (comma) {
            *comma = '\0';
            fields[count] = str_dup(p);
            if (!fields[count]) {
                for (int i = 0; i < count; i++) free(fields[i]);
                return 0;
            }
            count++;
            p = comma + 1;
        } else {
            fields[count] = str_dup(p);
            if (!fields[count]) {
                for (int i = 0; i < count; i++) free(fields[i]);
                return 0;
            }
            count++;
            break;
        }
    }

    return count;
}

static void free_fields(char *fields[], int count) {
    for (int i = 0; i < count; i++) {
        free(fields[i]);
        fields[i] = NULL;
    }
}

static int load_csv(const char *filename, Table *t) {
    if (!filename || !t) return 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening CSV");
        return 0;
    }

    free_table(t);
    init_table(t);

    char line[MAX_LINE_LEN];

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        printf("CSV file is empty.\n");
        return 0;
    }
    trim_newline(line);

    char *fields[MAX_COLS] = {0};
    int field_count = parse_csv_line(line, fields, MAX_COLS);
    if (field_count <= 0) {
        fclose(f);
        printf("Failed to parse header line.\n");
        return 0;
    }
    t->col_count = field_count;
    for (int i = 0; i < field_count; i++) {
        t->col_names[i] = fields[i];
    }

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (line[0] == '\0') continue;
        if (t->row_count >= MAX_ROWS) {
            printf("Reached max rows (%d). Remaining lines are ignored.\n", MAX_ROWS);
            break;
        }

        int count = parse_csv_line(line, fields, MAX_COLS);
        if (count <= 0) {
            printf("Skipping invalid row: %s\n", line);
            continue;
        }

        Row *r = &t->rows[t->row_count];
        init_row(r);
        r->cell_count = count;
        for (int i = 0; i < count; i++) {
            r->cells[i] = fields[i];
        }
        t->row_count++;
    }

    fclose(f);
    printf("Loaded %d rows with %d columns from '%s'.\n",
           t->row_count, t->col_count, filename);
    return 1;
}

static void show_summary(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    printf("\n=== CSV Summary ===\n");
    printf("Rows:   %d\n", t->row_count);
    printf("Cols:   %d\n", t->col_count);
    printf("Header: ");
    for (int i = 0; i < t->col_count; i++) {
        printf("%s", t->col_names[i] ? t->col_names[i] : "(col)");
        if (i + 1 < t->col_count) printf(", ");
    }
    printf("\n===================\n");
}

static void view_first_n(const Table *t, int n) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (n <= 0 || n > t->row_count) n = t->row_count;
    printf("\n-- First %d row(s) --\n", n);
    print_header(t);
    for (int i = 0; i < n; i++) {
        print_row(t, &t->rows[i]);
    }
}

static void view_last_n(const Table *t, int n) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (n <= 0 || n > t->row_count) n = t->row_count;
    int start = t->row_count - n;
    printf("\n-- Last %d row(s) --\n", n);
    print_header(t);
    for (int i = start; i < t->row_count; i++) {
        print_row(t, &t->rows[i]);
    }
}

static void insert_row(Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (t->row_count >= MAX_ROWS) {
        printf("Table is full.\n");
        return;
    }

    Row *r = &t->rows[t->row_count];
    init_row(r);
    r->cell_count = t->col_count;

    char buf[MAX_FIELD_LEN];
    printf("Inserting new row:\n");
    for (int i = 0; i < t->col_count; i++) {
        printf("Enter value for column '%s': ", t->col_names[i]);
        read_line_stdin(buf, sizeof(buf));
        r->cells[i] = str_dup(buf);
    }
    t->row_count++;
    printf("Row inserted at index %d.\n", t->row_count - 1);
}

static int find_row_index_by_value(const Table *t, int col_index, const char *value) {
    if (!t || col_index < 0 || col_index >= t->col_count || !value) return -1;
    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col_index < t->rows[i].cell_count && t->rows[i].cells[col_index])
                           ? t->rows[i].cells[col_index] : "";
        if (strcmp(cell, value) == 0) {
            return i;
        }
    }
    return -1;
}

static void delete_one_row(Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index for condition (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }
    printf("Enter value to match: ");
    char value[MAX_FIELD_LEN];
    read_line_stdin(value, sizeof(value));

    int idx = find_row_index_by_value(t, col, value);
    if (idx < 0) {
        printf("No row found where col[%d] = '%s'.\n", col, value);
        return;
    }

    printf("Deleting row %d:\n", idx);
    print_row(t, &t->rows[idx]);

    free_row(&t->rows[idx]);
    for (int i = idx; i < t->row_count - 1; i++) {
        t->rows[i] = t->rows[i + 1];
    }
    init_row(&t->rows[t->row_count - 1]);
    t->row_count--;
    printf("Row deleted.\n");
}

static void update_one_row(Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index for condition (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }
    printf("Enter value to match: ");
    char value[MAX_FIELD_LEN];
    read_line_stdin(value, sizeof(value));

    int idx = find_row_index_by_value(t, col, value);
    if (idx < 0) {
        printf("No row found where col[%d] = '%s'.\n", col, value);
        return;
    }

    Row *r = &t->rows[idx];
    printf("Current row:\n");
    print_row(t, r);

    printf("Enter new values (leave empty to keep current):\n");
    for (int i = 0; i < t->col_count; i++) {
        const char *current = (i < r->cell_count && r->cells[i]) ? r->cells[i] : "";
        printf("Column '%s' [%s]: ", t->col_names[i], current);
        read_line_stdin(buf, sizeof(buf));
        if (strlen(buf) > 0) {
            free(r->cells[i]);
            r->cells[i] = str_dup(buf);
        }
    }

    printf("Row updated:\n");
    print_row(t, r);
}

static void find_rows_by_value(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index to search (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }
    printf("Enter value to search: ");
    char value[MAX_FIELD_LEN];
    read_line_stdin(value, sizeof(value));

    int found = 0;
    print_header(t);
    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col < t->rows[i].cell_count && t->rows[i].cells[col])
                           ? t->rows[i].cells[col] : "";
        if (strcmp(cell, value) == 0) {
            print_row(t, &t->rows[i]);
            found = 1;
        }
    }
    if (!found) {
        printf("No rows found.\n");
    }
}

int find_rows_by_substring(const Table *t,
                           int col,
                           const char *pattern,
                           int out_indices[],
                           int max_out) {
    if (!t || !pattern || !out_indices || max_out <= 0) return 0;
    if (col < 0 || col >= t->col_count) return 0;
    if (pattern[0] == '\0') return 0;

    int count = 0;

    for (int i = 0; i < t->row_count; i++) {
        const Row *r = &t->rows[i];
        const char *cell = (col < r->cell_count && r->cells[col])
                           ? r->cells[col] : "";

        if (strstr(cell, pattern) != NULL) {
            if (count < max_out) {
                out_indices[count] = i;
            }
            count++;
        }
    }

    return count;
}

int find_rows_in_range(const Table *t,
                       int col,
                       double min_val,
                       double max_val,
                       int out_indices[],
                       int max_out) {
    if (!t || !out_indices || max_out <= 0) return 0;
    if (col < 0 || col >= t->col_count) return 0;

    if (min_val > max_val) {
        double tmp = min_val;
        min_val = max_val;
        max_val = tmp;
    }

    int count = 0;

    for (int i = 0; i < t->row_count; i++) {
        const Row *r = &t->rows[i];
        const char *cell = (col < r->cell_count && r->cells[col])
                           ? r->cells[col] : "";

        double v;
        if (!parse_double(cell, &v)) {
            continue;
        }

        if (v >= min_val && v <= max_val) {
            if (count < max_out) {
                out_indices[count] = i;
            }
            count++;
        }
    }

    return count;
}

static void find_rows_like(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }

    char buf[64];
    printf("Enter column index for LIKE (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    char pattern[MAX_FIELD_LEN];
    printf("Enter substring pattern: ");
    read_line_stdin(pattern, sizeof(pattern));

    if (pattern[0] == '\0') {
        printf("Empty pattern; nothing to search.\n");
        return;
    }

    int indices[MAX_ROWS];
    int count = find_rows_by_substring(t, col, pattern, indices, MAX_ROWS);

    if (count == 0) {
        printf("No rows matched pattern '%s' in column %d.\n", pattern, col);
        return;
    }

    printf("\nRows where col[%d] CONTAINS \"%s\":\n", col, pattern);
    print_header(t);
    int to_print = (count < MAX_ROWS) ? count : MAX_ROWS;
    for (int i = 0; i < to_print; i++) {
        print_row(t, &t->rows[indices[i]]);
    }
    if (count > MAX_ROWS) {
        printf("(Only first %d matches shown; total matches: %d)\n",
               MAX_ROWS, count);
    }
}

static void find_rows_between(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }

    char buf[64];
    printf("Enter column index for BETWEEN (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    char min_str[64], max_str[64];
    double min_val, max_val;

    printf("Enter MIN value: ");
    read_line_stdin(min_str, sizeof(min_str));
    if (!parse_double(min_str, &min_val)) {
        printf("Invalid MIN value.\n");
        return;
    }

    printf("Enter MAX value: ");
    read_line_stdin(max_str, sizeof(max_str));
    if (!parse_double(max_str, &max_val)) {
        printf("Invalid MAX value.\n");
        return;
    }

    int indices[MAX_ROWS];
    int count = find_rows_in_range(t, col, min_val, max_val, indices, MAX_ROWS);

    if (count == 0) {
        if (min_val > max_val) {
            double tmp = min_val;
            min_val = max_val;
            max_val = tmp;
        }
        printf("No rows found with col[%d] in [%.3f, %.3f].\n",
               col, min_val, max_val);
        return;
    }

    if (min_val > max_val) {
        double tmp = min_val;
        min_val = max_val;
        max_val = tmp;
    }

    printf("\nRows where col[%d] is BETWEEN %.3f AND %.3f:\n",
           col, min_val, max_val);
    print_header(t);
    int to_print = (count < MAX_ROWS) ? count : MAX_ROWS;
    for (int i = 0; i < to_print; i++) {
        print_row(t, &t->rows[indices[i]]);
    }
    if (count > MAX_ROWS) {
        printf("(Only first %d matches shown; total matches: %d)\n",
               MAX_ROWS, count);
    }
}

static void max_by_column(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index for MAX (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    int best_idx = -1;
    double best_val = 0.0;

    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col < t->rows[i].cell_count && t->rows[i].cells[col])
                           ? t->rows[i].cells[col] : "";
        double v;
        if (!parse_double(cell, &v)) continue;
        if (best_idx == -1 || v > best_val) {
            best_idx = i;
            best_val = v;
        }
    }

    if (best_idx == -1) {
        printf("No numeric values in column %d.\n", col);
    } else {
        printf("Row with MAX col[%d]=%.3f:\n", col, best_val);
        print_header(t);
        print_row(t, &t->rows[best_idx]);
    }
}

static void sum_avg_column(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }

    char buf[64];
    printf("Enter column index for SUM/AVG (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    double sum = 0.0;
    int count = 0;
    int non_numeric = 0;

    for (int i = 0; i < t->row_count; i++) {
        const Row *r = &t->rows[i];
        const char *cell = (col < r->cell_count && r->cells[col])
                           ? r->cells[col] : "";

        double v;
        if (parse_double(cell, &v)) {
            sum += v;
            count++;
        } else {
            if (cell[0] != '\0') {
                non_numeric++;
            }
        }
    }

    if (count == 0) {
        printf("No numeric values found in column %d.\n", col);
        return;
    }

    double avg = sum / (double)count;

    printf("\nSUM/AVG for column %d (%s):\n",
           col, t->col_names[col] ? t->col_names[col] : "(col)");
    printf("Numeric cells: %d\n", count);
    printf("Sum: %.6f\n", sum);
    printf("Avg: %.6f\n", avg);
    if (non_numeric > 0) {
        printf("Non-numeric (ignored) cells: %d\n", non_numeric);
    }
    printf("\n");
}

static void check_column_unique(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }

    char buf[64];
    printf("Enter column index to check for duplicates (0..%d): ",
           t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    int has_duplicates = 0;

    printf("\nChecking duplicates in column %d (%s):\n",
           col, t->col_names[col] ? t->col_names[col] : "(col)");

    for (int i = 0; i < t->row_count; i++) {
        const Row *ri = &t->rows[i];
        const char *vi = (col < ri->cell_count && ri->cells[col])
                         ? ri->cells[col] : "";

        if (vi[0] == '\0') {
            continue;
        }

        for (int j = i + 1; j < t->row_count; j++) {
            const Row *rj = &t->rows[j];
            const char *vj = (col < rj->cell_count && rj->cells[col])
                             ? rj->cells[col] : "";

            if (strcmp(vi, vj) == 0) {
                if (!has_duplicates) {
                    printf("Duplicates found:\n");
                }
                has_duplicates = 1;
                printf("  Value '%s' at rows %d and %d\n", vi, i, j);
            }
        }
    }

    if (!has_duplicates) {
        printf("No duplicates; column %d can be a UNIQUE / PRIMARY KEY.\n", col);
    }

    printf("\n");
}

static void min_by_column(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index for MIN (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    int best_idx = -1;
    double best_val = 0.0;

    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col < t->rows[i].cell_count && t->rows[i].cells[col])
                           ? t->rows[i].cells[col] : "";
        double v;
        if (!parse_double(cell, &v)) continue;
        if (best_idx == -1 || v < best_val) {
            best_idx = i;
            best_val = v;
        }
    }

    if (best_idx == -1) {
        printf("No numeric values in column %d.\n", col);
    } else {
        printf("Row with MIN col[%d]=%.3f:\n", col, best_val);
        print_header(t);
        print_row(t, &t->rows[best_idx]);
    }
}

static int compare_rows_by_col(const Table *t, const Row *a, const Row *b, int col, int asc) {
    const char *ca = (col < a->cell_count && a->cells[col]) ? a->cells[col] : "";
    const char *cb = (col < b->cell_count && b->cells[col]) ? b->cells[col] : "";

    double va, vb;
    int na = parse_double(ca, &va);
    int nb = parse_double(cb, &vb);
    int cmp;
    if (na && nb) {
        if (va < vb) cmp = -1;
        else if (va > vb) cmp = 1;
        else cmp = 0;
    } else {
        cmp = strcmp(ca, cb);
    }
    return asc ? cmp : -cmp;
}

static void sort_by_column(Table *t, int col, int asc) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }
    if (t->row_count <= 1) return;

    for (int i = 0; i < t->row_count - 1; i++) {
        for (int j = 0; j < t->row_count - 1 - i; j++) {
            if (compare_rows_by_col(t, &t->rows[j], &t->rows[j + 1], col, asc) > 0) {
                Row tmp = t->rows[j];
                t->rows[j] = t->rows[j + 1];
                t->rows[j + 1] = tmp;
            }
        }
    }
    printf("Sorted by column %d (%s).\n", col, asc ? "ASC" : "DESC");
}

typedef struct {
    char *value;
    int count;
} GroupEntry;

static void group_by_column(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index to GROUP BY (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    GroupEntry groups[MAX_ROWS];
    int group_count = 0;

    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col < t->rows[i].cell_count && t->rows[i].cells[col])
                           ? t->rows[i].cells[col] : "";
        int found = -1;
        for (int g = 0; g < group_count; g++) {
            if (strcmp(groups[g].value, cell) == 0) {
                found = g;
                break;
            }
        }
        if (found >= 0) {
            groups[found].count++;
        } else {
            if (group_count >= MAX_ROWS) {
                printf("Too many distinct groups; truncating.\n");
                break;
            }
            groups[group_count].value = (char *)cell; /* just reference; do not free */
            groups[group_count].count = 1;
            group_count++;
        }
    }

    printf("\nGROUP BY col[%d] (%s):\n", col,
           t->col_names[col] ? t->col_names[col] : "(col)");
    printf("Value | Count\n");
    printf("--------------\n");
    for (int g = 0; g < group_count; g++) {
        printf("%s | %d\n", groups[g].value, groups[g].count);
    }
}

static void show_distinct_values(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }

    char buf[64];
    printf("Enter column index for DISTINCT (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    const char *seen[MAX_ROWS];
    int seen_count = 0;

    for (int i = 0; i < t->row_count; i++) {
        const Row *r = &t->rows[i];
        const char *cell = (col < r->cell_count && r->cells[col])
                           ? r->cells[col] : "";

        int already = 0;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(seen[j], cell) == 0) {
                already = 1;
                break;
            }
        }
        if (!already) {
            if (seen_count < MAX_ROWS) {
                seen[seen_count++] = cell;
            } else {
                printf("Too many distinct values; truncating.\n");
                break;
            }
        }
    }

    printf("\nDISTINCT values of column %d (%s):\n",
           col, t->col_names[col] ? t->col_names[col] : "(col)");
    for (int i = 0; i < seen_count; i++) {
        printf("%s\n", seen[i]);
    }
    printf("Total distinct values: %d\n", seen_count);
}

static void save_csv(const char *filename, const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error opening output CSV");
        return;
    }
    for (int i = 0; i < t->col_count; i++) {
        fprintf(f, "%s", t->col_names[i] ? t->col_names[i] : "");
        if (i + 1 < t->col_count) fputc(',', f);
    }
    fputc('\n', f);
    for (int i = 0; i < t->row_count; i++) {
        Row const *r = &t->rows[i];
        for (int c = 0; c < t->col_count; c++) {
            const char *cell = (c < r->cell_count && r->cells[c]) ? r->cells[c] : "";
            fprintf(f, "%s", cell);
            if (c + 1 < t->col_count) fputc(',', f);
        }
        fputc('\n', f);
    }
    fclose(f);
    printf("Saved table to '%s'.\n", filename);
}

static void print_menu(void) {
    printf("\n=========== CSV-SQL MENU ===========\n");
    printf("1. Load CSV file\n");
    printf("2. Show CSV summary\n");
    printf("3. View first N rows\n");
    printf("4. View last N rows\n");
    printf("5. Insert 1 row\n");
    printf("6. Delete 1 row (WHERE col = value)\n");
    printf("7. Update 1 row (WHERE col = value)\n");
    printf("8. Find rows by value (WHERE col = value)\n");
    printf("9. MAX by column (numeric)\n");
    printf("10. MIN by column (numeric)\n");
    printf("11. SUM / AVG of numeric column\n");
    printf("12. Check column for duplicate values\n");
    printf("13. Sort ASC by column\n");
    printf("14. Sort DESC by column\n");
    printf("15. GROUP BY column\n");
    printf("16. DISTINCT values of a column\n");
    printf("17. Find rows where column CONTAINS substring (LIKE)\n");
    printf("18. Find rows where numeric column is BETWEEN min and max\n");
    printf("19. Save table to CSV\n");
    printf("20. Exit\n");

    printf("====================================\n");
    printf("Enter choice: ");
}

int main(void) {
    Table table;
    init_table(&table);

    char buf[128];
    int running = 1;

    while (running) {
        print_menu();
        read_line_stdin(buf, sizeof(buf));
        int choice = atoi(buf);

        switch (choice) {
            case 1: {
                char filename[256];
                printf("Enter CSV filename: ");
                read_line_stdin(filename, sizeof(filename));
                if (filename[0] == '\0') {
                    printf("No filename.\n");
                } else {
                    load_csv(filename, &table);
                }
                break;
            }
            case 2:{
                show_summary(&table);
                break;
                }
            case 3: {
                printf("Enter N: ");
                read_line_stdin(buf, sizeof(buf));
                int n = atoi(buf);
                view_first_n(&table, n);
                break;
            }
            case 4: {
                printf("Enter N: ");
                read_line_stdin(buf, sizeof(buf));
                int n = atoi(buf);
                view_last_n(&table, n);
                break;
            }
            case 5:{
                insert_row(&table);
                break;
                }
            case 6:{
                delete_one_row(&table);
                break;
                }
            case 7:{
                update_one_row(&table);
                break;
                }
            case 8:{
                find_rows_by_value(&table);
                break;
                }
            case 9:{
                max_by_column(&table);
                break;
                }
            case 10:{
                min_by_column(&table);
                break;
                }
            case 11:{
                sum_avg_column(&table);
                break;
                }
            case 12:{
                check_column_unique(&table);
                break;
                }
            case 13: {
                printf("Enter column index for ASC sort: ");
                read_line_stdin(buf, sizeof(buf));
                int col = atoi(buf);
                sort_by_column(&table, col, 1);
                break;
            }
            case 14: {
                printf("Enter column index for DESC sort: ");
                read_line_stdin(buf, sizeof(buf));
                int col = atoi(buf);
                sort_by_column(&table, col, 0);
                break;
            }
            case 15:{
                group_by_column(&table);
                break;
                }
            case 16:{
                show_distinct_values(&table);
                break;
                }
            case 17: {
                find_rows_like(&table);
                break;
            }
            case 18: {
                find_rows_between(&table);
                break;
            }
            case 19: {
                char filename[256];
                printf("Enter filename to save CSV: ");
                read_line_stdin(filename, sizeof(filename));
                if (filename[0] == '\0') {
                    printf("No filename.\n");
                } else {
                    save_csv(filename, &table);
                }
                break;
            }
            case 20:{
                running = 0;
                break;
                }
            default:
                printf("Invalid choice.\n");
                break;
        }
    }

    free_table(&table);
    return 0;
}
