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

/* forward declares from your real project */
void init_row(Row *r);
char *str_dup(const char *s);
void read_line_stdin(char *buf, size_t size);

static void insert_row(Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (t->row_count >= MAX_ROWS) {
        printf("Table is full.\n");
        return;
    }

    Row *r = &t->rows[t->row_count];
    init_row(r);
    r->cell_count = t->col_count;

    char buf[MAX_FIELD_LEN];
    printf("Inserting new row:\n");
    for (int i = 0; i < t->col_count; i++) {
        printf("Enter value for column '%s': ", t->col_names[i]);
        read_line_stdin(buf, sizeof(buf));
        r->cells[i] = str_dup(buf);
    }
    t->row_count++;
    printf("Row inserted at index %d.\n", t->row_count - 1);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Redirect stdout -> /dev/null */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build a fake table */
    Table t;
    t.col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    if (t.col_count == 0) t.col_count = 1; /* ensure at least 1 column */
    t.row_count = 0;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Create fake stdin using fuzz data */
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

    /* Call target function */
    insert_row(&t);

    /* Restore stdio */
    stdin = orig_stdin;
    stdout = orig_stdout;

    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Free allocated column names */
    for (int i = 0; i < t.col_count; i++) {
        free(t.col_names[i]);
    }

    /* Free row data */
    for (int i = 0; i < t.row_count; i++) {
        for (int j = 0; j < t.rows[i].cell_count; j++) {
            free(t.rows[i].cells[j]);
        }
    }

    return 0;
}
