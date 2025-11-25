#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import the REAL project implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Minimum 1 byte required */
    if (size == 0)
        return 0;

    /* ---- Create null-terminated fuzz string ---- */
    char *input = malloc(size + 1);
    if (!input) return 0;

    memcpy(input, data, size);
    input[size] = '\0';

    /* ---- Prepare storage for output fields ---- */
    const int MAX_FIELDS = MAX_COLS;  /* project uses MAX_COLS */
    char *fields[MAX_FIELDS];
    memset(fields, 0, sizeof(fields));

    /* fuzz max_fields as well */
    int maxf = 1 + (data[0] % MAX_FIELDS);

    /* ---- Call the real function from csv_sql.c ---- */
    int count = parse_csv_line(input, fields, maxf);

    /* ---- Free any allocated fields ---- */
    for (int i = 0; i < count && i < maxf; i++) {
        free(fields[i]);
    }

    free(input);
    return 0;
}
