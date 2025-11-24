#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// The function under test (include your actual header normally)
static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

// LibFuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Guarantee space for a null terminator
    char *buf = (char *)malloc(size + 1);
    if (!buf) return 0;

    // Copy fuzz input as raw bytes (libFuzzer gives arbitrary binary data)
    memcpy(buf, data, size);

    // Ensure null-terminated string
    buf[size] = '\0';

    // Call the function
    trim_newline(buf);

    free(buf);
    return 0;
}
