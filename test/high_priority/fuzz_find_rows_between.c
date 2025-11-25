#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

/* external project functions */
void read_line_stdin(char *buf, size_t size);
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);
int find_rows_in_range(const Table *t, int col, double minv, double maxv,
                       int *out_indices, int max_out);
int parse_double(const char *s, double *out);

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
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 5) return 0;

    /* suppress stdout */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* ==== Build a valid table ==== */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);
    if (t.row_count == 0) t.row_count = 1;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Allocate cells */
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

    /* ==== Fake stdin input ==== */
    /* data stream becomes:
         <col index>\n
         <min value>\n
         <max value>\n
    */

    char *input = malloc(size + 3);
    memcpy(input, data, size);
    input[size] = '\n';
    input[size+1] = '\n';
    input[size+2] = '\0';

    FILE *fake_stdin = fmemopen(input, size+3, "r");
    if (!fake_stdin) {
        free(input);
        return 0;
    }

    FILE *orig_stdin = stdin;
    stdin = fake_stdin;

    /* ==== Run the target ==== */
    find_rows_between(&t);

    /* ==== restore ==== */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* ==== free memory ==== */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
