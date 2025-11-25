#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 1024
#define MAX_LINE_LEN 1024

/* bring real structs + functions from your project */
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

/* forward declarations from your project */
void init_row(Row *r);
void init_table(Table *t);
void free_row(Row *r);
void free_table(Table *t);
void trim_newline(char *s);
int parse_csv_line(const char *line, char *fields[], int max_fields);

int load_csv(const char *filename, Table *t);


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* --- Step 1: Write fuzz data into a temporary file --- */
    const char *tmpname = "/tmp/fuzz_input.csv";
    FILE *fp = fopen(tmpname, "wb");
    if (!fp) return 0;

    fwrite(data, 1, size, fp);
    fclose(fp);

    /* --- Step 2: Prepare a table struct --- */
    Table t;
    init_table(&t);

    /* --- Step 3: Call the function under test --- */
    load_csv(tmpname, &t);

    /* --- Step 4: Cleanup --- */
    free_table(&t);

    return 0;
}
