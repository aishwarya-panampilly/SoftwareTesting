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

/* external functions from your project */
void read_line_stdin(char *buf, size_t size);

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
        if (!already && seen_count < MAX_ROWS) {
            seen[seen_count++] = cell;
        }
    }

    printf("\nDISTINCT values of column %d (%s):\n",
           col, t->col_names[col] ? t->col_names[col] : "(col)");
    for (int i = 0; i < seen_count; i++) {
        printf("%s\n", seen[i]);
    }
    printf("Total distinct values: %d\n", seen_count);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 3) return 0;

    /* Redirect stdout to suppress noise */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build valid Table */
    Table t;

    t.col_count = (data[0] % (MAX_COLS + 1));
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (data[1] % (MAX_ROWS + 1));
    if (t.row_count == 0) t.row_count = 1;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Allocate each cell as a safe string */
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

    /* Fake stdin input using fuzz data */
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

    /* Run the target */
    show_distinct_values(&t);

    /* Restore stdio */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Free memory */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
