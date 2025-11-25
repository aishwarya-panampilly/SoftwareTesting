#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Function under test (normally include the real header)
static int parse_double(const char *s, double *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Create null-terminated string from fuzz input
    char *input = malloc(size + 1);
    if (!input) return 0;
    memcpy(input, data, size);
    input[size] = '\0';

    double out_value = 0.0;

    // Call the function being fuzzed
    (void)parse_double(input, &out_value);

    free(input);
    return 0;
}
