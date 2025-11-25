#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COLS 32

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

static void free_row(Row *r) {
    if (!r) return;
    for (int i = 0; i < r->cell_count; i++) {
        free(r->cells[i]);
        r->cells[i] = NULL;
    }
    r->cell_count = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    Row r;

    // Number of cells to allocate = fuzz_input[0] % (MAX_COLS + 1)
    int num_cells = 0;
    if (size > 0)
        num_cells = data[0] % (MAX_COLS + 1);

    r.cell_count = num_cells;

    // For each cell, allocate memory safely and fill with fuzz content
    size_t chunk_size = size > 1 ? (size - 1) / (num_cells > 0 ? num_cells : 1) : 1;

    for (int i = 0; i < num_cells; i++) {
        size_t alloc_size = chunk_size > 0 ? chunk_size : 1;

        // Allocate string buffer
        r.cells[i] = (char *)malloc(alloc_size);
        if (!r.cells[i]) return 0;

        // Fill cell content with fuzz data slice
        memcpy(r.cells[i], data + 1 + i * chunk_size,
               (1 + i * chunk_size + alloc_size <= size)
                   ? alloc_size
                   : size - (1 + i * chunk_size));
    }

    // Initialize any unused cell pointers to NULL
    for (int i = num_cells; i < MAX_COLS; i++)
        r.cells[i] = NULL;

    // Call function under test
    free_row(&r);

    return 0;
}
