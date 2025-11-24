#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void free_fields(char *fields[], int count) {
    for (int i = 0; i < count; i++) {
        free(fields[i]);
        fields[i] = NULL;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    const int MAX_FIELDS = 64;
    char *fields[MAX_FIELDS];

    // Determine count using fuzz input
    int count = 0;
    if (size > 0)
        count = data[0] % (MAX_FIELDS + 1);

    // Allocate real memory for each field
    for (int i = 0; i < count; i++) {
        size_t alloc_size = 4 + ((i + (size > 1 ? data[i % size] : 0)) % 32);
        fields[i] = malloc(alloc_size);
        if (fields[i]) {
            // Fill with fuzz data, null-terminate
            size_t copy_len = alloc_size - 1;
            memcpy(fields[i],
                   &data[(1 + i) % size],
                   (copy_len <= size ? copy_len : size));
            fields[i][alloc_size - 1] = '\0';
        }
    }

    // Null the rest
    for (int i = count; i < MAX_FIELDS; i++) {
        fields[i] = NULL;
    }

    // Call the function under test
    free_fields(fields, count);

    return 0;
}
