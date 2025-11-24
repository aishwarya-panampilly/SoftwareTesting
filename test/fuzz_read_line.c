#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// The function under test (normally included from your header)
static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static void read_line_stdin(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

// libFuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Create a buffer for input (null-terminated for safety)
    char *input = (char *)malloc(size + 1);
    if (!input) return 0;
    memcpy(input, data, size);
    input[size] = '\0';

    // Create a fake FILE* stream from fuzz data
    FILE *fake = fmemopen(input, size, "r");
    if (!fake) {
        free(input);
        return 0;
    }

    // Save original stdin
    FILE *orig_stdin = stdin;
    stdin = fake;

    // Prepare an output buffer for the function
    char outbuf[1024];
    memset(outbuf, 0, sizeof(outbuf));

    // Call the function under fuzzing
    read_line_stdin(outbuf, sizeof(outbuf));

    // Restore stdin
    stdin = orig_stdin;

    fclose(fake);
    free(input);

    return 0;
}
