#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

/* real project functions */
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);
void read_line_stdin(char *buf, size_t size);
int parse_double(const char *s, double *out);

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
        const char *cell = (col < t->rows[i].cell_count &&
                            t->rows[i].cells[col])
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* ----- Redirect stdout ----- */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* ----- Build valid Table ----- */
    Table t;

    t.col_count = (size > 0 ? data[0] % (MAX_COLS + 1) : 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (size > 1 ? data[1] % (MAX_ROWS + 1) : 1);

    /* allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* allocate rows and cell values */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            /* randomize cell size */
            size_t alloc = 4 + ((r + c) % 32);

            t.rows[r].cells[c] = malloc(alloc);
            if (t.rows[r].cells[c]) {
                size_t copy_len = alloc - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(2 + r + c) % size],
                       (copy_len <= size ? copy_len : size));
                t.rows[r].cells[c][alloc - 1] = '\0';
            }
        }
    }

    /* ----- Create fake stdin from fuzz data ----- */
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

    /* ----- Run function under test ----- */
    max_by_column(&t);

    /* ----- Restore stdin/stdout ----- */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* ----- Cleanup memory ----- */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
