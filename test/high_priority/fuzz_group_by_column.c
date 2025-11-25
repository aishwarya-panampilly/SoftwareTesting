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

typedef struct {
    char *value;
    int count;
} GroupEntry;

/* external real functions */
void read_line_stdin(char *buf, size_t size);

static void group_by_column(const Table *t) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    char buf[64];
    printf("Enter column index to GROUP BY (0..%d): ", t->col_count - 1);
    read_line_stdin(buf, sizeof(buf));
    int col = atoi(buf);
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }

    GroupEntry groups[MAX_ROWS];
    int group_count = 0;

    for (int i = 0; i < t->row_count; i++) {
        const char *cell = (col < t->rows[i].cell_count && t->rows[i].cells[col])
                           ? t->rows[i].cells[col] : "";
        int found = -1;
        for (int g = 0; g < group_count; g++) {
            if (strcmp(groups[g].value, cell) == 0) {
                found = g;
                break;
            }
        }
        if (found >= 0) {
            groups[found].count++;
        } else {
            if (group_count >= MAX_ROWS) {
                printf("Too many distinct groups; truncating.\n");
                break;
            }
            groups[group_count].value = (char *)cell;
            groups[group_count].count = 1;
            group_count++;
        }
    }

    printf("\nGROUP BY col[%d] (%s):\n", col,
           t->col_names[col] ? t->col_names[col] : "(col)");
    printf("Value | Count\n--------------\n");

    for (int g = 0; g < group_count; g++) {
        printf("%s | %d\n", groups[g].value, groups[g].count);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
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

    /* Allocate row cells */
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

    /* Fake stdin using fuzz input */
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

    /* Run target */
    group_by_column(&t);

    /* Restore stdio */
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
