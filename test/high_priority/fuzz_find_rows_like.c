#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 1024
#define MAX_FIELD_LEN 256

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

typedef struct {
    int col_count;
    int row_count;
    char *col_names[MAX_COLS];
    Row rows[MAX_ROWS];
} Table;

/* external from project */
void read_line_stdin(char *buf, size_t size);
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);
int find_rows_by_substring(const Table *t, int col, const char *pattern,
                           int *out_indices, int max_out);

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
        printf("(Only first %d shown; total: %d)\n", MAX_ROWS, count);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) return 0;

    /* Silence output */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build safe table */
    Table t;

    t.col_count = (data[0] % (MAX_COLS + 1));
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (data[1] % (MAX_ROWS + 1));
    if (t.row_count == 0) t.row_count = 1;

    /* Column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Rows and cells */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;
        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t len = alloc - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(2 + r + c) % size],
                       (len <= size ? len : size));
                t.rows[r].cells[c][alloc - 1] = '\0';
            }
        }
    }

    /* Fake stdin input */
    char *input = malloc(size + 1);
    memcpy(input, data, size);
    input[size] = '\0';

    FILE *fake_stdin = fmemopen(input, size, "r");
    if (!fake_stdin) {
        free(input);
        return 0;
    }

    FILE *orig_stdin = stdin;
    stdin = fake_stdin;

    /* Execute target */
    find_rows_like(&t);

    /* Restore IO */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Free */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
