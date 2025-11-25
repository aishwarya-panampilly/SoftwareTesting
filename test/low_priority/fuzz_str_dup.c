#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Function under test (normally you'd include the real header instead)
static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);
    return p;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Allocate + null-terminate fuzz input to make it a valid C string.
    char *input = (char *)malloc(size + 1);
    if (!input) return 0;

    memcpy(input, data, size);
    input[size] = '\0';  // required because strlen() is used

    // Call the function under test
    char *out = str_dup(input);

    // Free allocations
    free(out);
    free(input);

    return 0;
}
