#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../csv_sql.c"   // import real parse_double()

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size == 0) return 0;

    char *input = malloc(size + 1);
    if (!input) return 0;

    memcpy(input, data, size);
    input[size] = '\0';

    double out_value = 0.0;
    parse_double(input, &out_value);   // <-- REAL project function

    free(input);
    return 0;
}
