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

/* External real functions */
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);
void read_line_stdin(char *buf, size_t size);
int parse_double(const char *s, double *out);

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


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Redirect stdout */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build valid table */
    Table t;

    t.col_count = (size > 0 ? data[0] % (MAX_COLS + 1) : 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (size > 1 ? data[1] % (MAX_ROWS + 1) : 1);

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Allocate row data */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
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

    /* Fake stdin for reading column index */
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

    /* Call target */
    sum_avg_column(&t);

    /* Restore */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Cleanup */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
