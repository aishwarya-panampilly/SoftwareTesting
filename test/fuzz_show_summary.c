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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Redirect stdout to /dev/null */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    Table t;

    /* Row/column counts come from fuzz input */
    t.col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    t.row_count = (size > 1) ? (data[1] % (MAX_ROWS + 1)) : 0;

    /* Allocate real strings for column names */
    for (int i = 0; i < t.col_count; i++) {
        size_t alloc_size = 4 + (size > 2 ? data[(2 + i) % size] % 32 : 8);
        t.col_names[i] = malloc(alloc_size);
        if (t.col_names[i]) {
            size_t copy_len = alloc_size - 1;
            memcpy(t.col_names[i],
                   &data[(2 + i) % size],
                   (copy_len <= size ? copy_len : size));
            t.col_names[i][alloc_size - 1] = '\0';
        }
    }
    for (int i = t.col_count; i < MAX_COLS; i++) {
        t.col_names[i] = NULL;
    }

    /* Call the function under test */
    show_summary(&t);

    /* Cleanup */
    for (int i = 0; i < t.col_count; i++) {
        free(t.col_names[i]);
    }

    stdout = orig_stdout;
    fclose(devnull);

    return 0;
}
