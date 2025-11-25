#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 256

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

static void print_header(const Table *t) {
    if (!t) return;
    for (int i = 0; i < t->col_count; i++) {
        printf("%s", t->col_names[i] ? t->col_names[i] : "(col)");
        if (i + 1 < t->col_count) printf(" | ");
    }
    printf("\n");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Redirect stdout → /dev/null for speed & no noise
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    Table t;

    // Determine number of columns using fuzz input
    int col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    t.col_count = col_count;

    // Allocate valid strings for column names
    for (int i = 0; i < col_count; i++) {
        // Ensure we always allocate a string with at least 4 chars
        size_t alloc_size = 4 + (size > 1 ? data[(1 + i) % size] % 32 : 8); // 4–36 bytes
        t.col_names[i] = malloc(alloc_size);
        if (t.col_names[i]) {
            // Copy fuzz bytes into the string, null-terminate
            size_t copy_len = alloc_size - 1;
            memcpy(t.col_names[i],
                   &data[(1 + i) % size],
                   (copy_len <= size ? copy_len : size));
            t.col_names[i][alloc_size - 1] = '\0';
        }
    }

    // Fill the rest of col_names with NULL
    for (int i = col_count; i < MAX_COLS; i++) {
        t.col_names[i] = NULL;
    }

    // Call function under test
    print_header(&t);

    // Cleanup allocated strings
    for (int i = 0; i < col_count; i++) {
        free(t.col_names[i]);
    }

    stdout = orig_stdout;
    fclose(devnull);

    return 0;
}
