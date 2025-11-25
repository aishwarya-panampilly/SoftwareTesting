#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* bring in your declarations here (or include your header) */
#define MAX_LINE_LEN 1024

static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);
    return p;
}

int parse_csv_line(const char *line, char *fields[], int max_fields) {
    if (!line || !fields || max_fields <= 0) return 0;

    char buffer[MAX_LINE_LEN];
    size_t len = strlen(line);
    if (len >= sizeof(buffer)) {
        return 0;
    }
    strcpy(buffer, line);

    int count = 0;
    char *p = buffer;
    while (count < max_fields) {
        char *comma = strchr(p, ',');
        if (comma) {
            *comma = '\0';
            fields[count] = str_dup(p);
            if (!fields[count]) {
                for (int i = 0; i < count; i++) free(fields[i]);
                return 0;
            }
            count++;
            p = comma + 1;
        } else {
            fields[count] = str_dup(p);
            if (!fields[count]) {
                for (int i = 0; i < count; i++) free(fields[i]);
                return 0;
            }
            count++;
            break;
        }
    }
    return count;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Guarantee null-terminated string for parser */
    char *input = malloc(size + 1);
    if (!input) return 0;

    memcpy(input, data, size);
    input[size] = '\0';

    /* Prepare output array */
    const int MAX_FIELDS = 64;
    char *fields[MAX_FIELDS] = {0};

    /* Fuzz max_fields also based on input */
    int maxf = 1;
    if (size > 0) {
        maxf = 1 + (data[0] % MAX_FIELDS);
    }

    /* Call the target function */
    int count = parse_csv_line(input, fields, maxf);

    /* Free allocated fields */
    for (int i = 0; i < count && i < maxf; i++) {
        free(fields[i]);
    }

    free(input);
    return 0;
}
