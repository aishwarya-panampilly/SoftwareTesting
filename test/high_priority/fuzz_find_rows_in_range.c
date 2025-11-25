#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 1024

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

/* from your project */
int find_rows_in_range(const Table *t,
                       int col,
                       double min_val,
                       double max_val,
                       int out_indices[],
                       int max_out);

int parse_double(const char *s, double *out);

/* ---------------------------- */
/*       FUZZER ENTRYPOINT      */
/* ---------------------------- */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    /* ------------------------- */
    /*   BUILD A VALID TABLE     */
    /* ------------------------- */

    Table t;

    t.col_count = (data[0] % MAX_COLS);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (data[1] % MAX_ROWS);
    if (t.row_count == 0) t.row_count = 1;

    /* allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* fill rows with fuzzer-controlled strings */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            size_t alloc_len = 4 + (data[(2 + r + c) % size] % 32);
            char *buf = malloc(alloc_len);
            if (buf) {
                size_t copy_len = alloc_len - 1;
                memcpy(buf,
                       &data[(3 + r + c) % size],
                       (copy_len < size ? copy_len : size));
                buf[alloc_len - 1] = '\0';
            }
            t.rows[r].cells[c] = buf;
        }
    }

    /* ------------------------- */
    /*  PICK RANGE AND COLUMN    */
    /* ------------------------- */

    int col_index = data[2] % t.col_count;

    /* fuzz numeric values */
    double min_val = 0;
    double max_val = 0;

    /* interpret two chunks of fuzz bytes as double-ish strings */
    char min_buf[32], max_buf[32];
    size_t half = size / 2;

    size_t min_len = (half < sizeof(min_buf) - 1) ? half : (sizeof(min_buf) - 1);
    memcpy(min_buf, data, min_len);
    min_buf[min_len] = '\0';

    size_t max_len = (size - half < sizeof(max_buf) - 1) ? (size - half) : (sizeof(max_buf) - 1);
    memcpy(max_buf, data + half, max_len);
    max_buf[max_len] = '\0';

    parse_double(min_buf, &min_val);
    parse_double(max_buf, &max_val);

    int *indices = malloc(MAX_ROWS * sizeof(int));

    /* ------------------------- */
    /*     CALL TARGET FUNC      */
    /* ------------------------- */

    find_rows_in_range(&t,
                       col_index,
                       min_val,
                       max_val,
                       indices,
                       MAX_ROWS);

    /* ------------------------- */
    /*         CLEANUP           */
    /* ------------------------- */

    free(indices);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
