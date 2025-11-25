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

/* real project function signatures */
void print_header(const Table *t);
void read_line_stdin(char *buf, size_t size);
void print_row(const Table *t, const Row *r);

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

        if (vi[0] == '\0') continue;

        for (int j = i + 1; j < t->row_count; j++) {
            const Row *rj = &t->rows[j];
            const char *vj = (col < rj->cell_count && rj->cells[col])
                             ? rj->cells[col] : "";

            if (strcmp(vi, vj) == 0) {
                if (!has_duplicates) printf("Duplicates found:\n");
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }
    /* Redirect stdout to /dev/null */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build safe table */
    Table t;

    t.col_count = (size > 0 ? data[0] % (MAX_COLS + 1) : 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (size > 1 ? data[1] % (MAX_ROWS + 1) : 1);

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Allocate table cells */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
	size_t alloc = 4 + ((r + c) % 32);  /* random-ish size */
	t.rows[r].cells[c] = malloc(alloc);
	if (t.rows[r].cells[c]) {

	    size_t copy_len = alloc - 1;  // leave space for null terminator

	    size_t src_index = (2 + r + c) % size;   // starting point inside data
	    size_t max_from_src = size - src_index;  // how many bytes left in data

	    // copy only as much as available and allocated
	    size_t to_copy = (copy_len < max_from_src) ? copy_len : max_from_src;

	    if (to_copy > 0) {
		memcpy(t.rows[r].cells[c], &data[src_index], to_copy);
	    }

	    t.rows[r].cells[c][to_copy] = '\0';  // safe null-termination
	}
        }
    }

    /* Provide fake stdin to simulate user input */
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

    /* Run function under test */
    check_column_unique(&t);

    /* Restore stdio */
    stdin = orig_stdin;
    stdout = orig_stdout;
    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Cleanup memory */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
