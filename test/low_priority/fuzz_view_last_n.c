#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

/* forward declarations from your code */
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);

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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Redirect stdout to /dev/null to avoid noise */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig = stdout;
    stdout = devnull;

    Table t;

    /* Fuzz-influenced counts */
    t.col_count = (size > 0 ? data[0] % (MAX_COLS + 1) : 0);
    t.row_count = (size > 1 ? data[1] % (MAX_ROWS + 1) : 0);

    /* ----- Allocate column names safely ----- */
    for (int i = 0; i < t.col_count; i++) {
        size_t alloc = 4 + (size > 2 ? data[(2 + i) % size] % 32 : 8);
        t.col_names[i] = malloc(alloc);

        if (t.col_names[i]) {
            size_t copy_len = alloc - 1;
            memcpy(t.col_names[i],
                   &data[(2 + i) % size],
                   (copy_len <= size ? copy_len : size));
            t.col_names[i][alloc - 1] = '\0';
        }
    }
    for (int i = t.col_count; i < MAX_COLS; i++)
        t.col_names[i] = NULL;

    /* ----- Construct row data ----- */
    for (int r = 0; r < t.row_count; r++) {
        int cell_cnt = (size > 3 ? data[(3 + r) % size] % (t.col_count + 1) : 0);
        t.rows[r].cell_count = cell_cnt;

        for (int c = 0; c < cell_cnt; c++) {
            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t copy_len = alloc - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(4 + r + c) % size],
                       (copy_len <= size ? copy_len : size));
                t.rows[r].cells[c][alloc - 1] = '\0';
            }
        }

        for (int c = cell_cnt; c < MAX_COLS; c++)
            t.rows[r].cells[c] = NULL;
    }

    /* ----- fuzz-controlled n ----- */
    int n = (size > 2) ? (int)(data[2] % (MAX_ROWS + 5)) : 1;

    /* ----- call function under test ----- */
    view_last_n(&t, n);

    /* ----- cleanup ----- */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.rows[r].cell_count; c++)
            free(t.rows[r].cells[c]);

    stdout = orig;
    fclose(devnull);

    return 0;
}
