#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Function under test (normally from your header)
#define MAX_COLS 32
typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

static void init_row(Row *r) {
    if (!r) return;
    r->cell_count = 0;
    for (int i = 0; i < MAX_COLS; i++) {
        r->cells[i] = NULL;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    Row r;

    // Fill Row with fuzz bytes to simulate corrupted/uninitialized input
    // This is safe because init_row overwrites everything.
    size_t copy_len = size < sizeof(Row) ? size : sizeof(Row);
    memset(&r, 0xAA, sizeof(Row));     // initialize with known garbage
    memcpy(&r, data, copy_len);        // overwrite with fuzz data

    // Call the function under test
    init_row(&r);

    // No need to free anything — Row contains only pointers, not allocations.
    return 0;
}
