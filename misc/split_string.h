#ifndef BADVPN_SPLIT_STRING_H
#define BADVPN_SPLIT_STRING_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include <misc/balloc.h>
#include <misc/debug.h>

static char ** split_string (const char *str, char del)
{
    size_t len = strlen(str);
    
    // count parts
    size_t num_parts = 0;
    size_t i = 0;
    while (1) {
        size_t j = i;
        while (j < len && str[j] != del) j++;
        
        num_parts++;
        
        if (num_parts == SIZE_MAX) { // need to allocate +1 pointer
            goto fail0;
        }
        
        if (j == len) {
            break;
        }
        
        i = j + 1;
    }
    
    // allocate array for part pointers
    char **result = BAllocArray(num_parts + 1, sizeof(*result));
    if (!result) {
        goto fail0;
    }
    
    num_parts = 0;
    i = 0;
    while (1) {
        size_t j = i;
        while (j < len && str[j] != del) j++;
        
        if (!(result[num_parts] = malloc(j - i + 1))) {
            goto fail1;
        }
        memcpy(result[num_parts], str + i, j - i);
        result[num_parts][j - i] = '\0';
        
        num_parts++;
        
        if (j == len) {
            break;
        }
        
        i = j + 1;
    }
    
    result[num_parts] = NULL;
    
    return result;
    
fail1:
    while (num_parts-- > 0) {
        free(result[num_parts]);
    }
    BFree(result);
fail0:
    return NULL;
}

static size_t count_strings (char **names)
{
    ASSERT(names)
    
    size_t i;
    for (i = 0; names[i]; i++);
    
    return i;
}

static void free_strings (char **names)
{
    ASSERT(names)
    
    size_t i = count_strings(names);
    
    while (i-- > 0) {
        free(names[i]);
    }
    
    BFree(names);
}

#endif
