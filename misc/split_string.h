#ifndef BADVPN_SPLIT_STRING_H
#define BADVPN_SPLIT_STRING_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include <misc/balloc.h>
#include <misc/debug.h>
#include <misc/expstring.h>

/**
 * Splits the given string by a character, and returns the result
 * as a malloc'd array of char pointers, each malloc'd. The array
 * is terminated by a NULL pointer.
 * 
 * @param str string to split
 * @param del delimiter character
 * @return pointer to array of strings, or NULL on failure. On success,
 *         at least one string will always be returned.
 */
static char ** split_string (const char *str, char del);

/**
 * Counts the number of strings in an array (same format as used by
 * {@link split_string}).
 * 
 * @param names pointer to array of strings; must not be NULL.
 * @return number of strings
 */
static size_t count_strings (char **names);

/**
 * Frees an array of strings (same format as used by
 * {@link split_string}). This first frees the individual strings,
 * then the whole array.
 * 
 * @param names pointer to array of strings; must not be NULL.
 */
static void free_strings (char **names);

/**
 * Concatenates the given strings, inserting a delimiter character
 * in between. The result is returned as a newly malloc'd string.
 * If there are no strings, an empty string is produced.
 * If there is just one string, this string is copied.
 * 
 * @param names pointer to array of strings; must not be NULL.
 *              Format is as returned by {@link split_string}.
 * @param del delimiter to insert between strings
 * @return resulting malloc'd string, or NULL on failure.
 */
static char * implode_strings (char **names, char del);

/**
 * Splits the given string by a character in-place by replacing
 * all delimiting characters with nulls, and returns the number
 * of such replacements.
 * 
 * @param str string to split in-place; must not be NULL.
 * @param del delimiter character
 * @return number of replaced characters, or equivalently, number
 *         of resulting strings minus one
 */
static size_t split_string_inplace2 (char *str, char del);

/**
 * Concatenates the given strings, inserting a delimiter character
 * in between. The result is returned as a newly malloc'd string.
 * If there are no strings, an empty string is produced.
 * If there is just one string, this string is copied.
 * 
 * @param names pointer to a character array containing the
 *              null-terinated strings one after another, i.e.
 *              as produced by {@link split_string_inplace2}.
 *              Must not be NULL.
 * @param num_names number of strings, e.g. the return value of
 *                  {@link split_string_inplace2} plus one
 * @param del delimiter to insert between strings
 * @return resulting malloc'd string, or NULL on failure.
 */
static char * implode_compact_strings (const char *names, size_t num_names, char del);

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

static char * implode_strings (char **names, char del)
{
    ASSERT(names)
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        goto fail0;
    }
    
    for (size_t i = 0; names[i]; i++) {
        if (i > 0 && !ExpString_AppendChar(&str, del)) {
            goto fail1;
        }
        if (!ExpString_Append(&str, names[i])) {
            goto fail1;
        }
    }
    
    return ExpString_Get(&str);
    
fail1:
    ExpString_Free(&str);
fail0:
    return NULL;
}

static size_t split_string_inplace2 (char *str, char del)
{
    ASSERT(str)
    
    size_t num_extra_parts = 0;
    
    while (*str) {
        if (*str == del) {
            *str = '\0';
            num_extra_parts++;
        }
        str++;
    }
    
    return num_extra_parts;
}

static char * implode_compact_strings (const char *names, size_t num_names, char del)
{
    ASSERT(names)
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        goto fail0;
    }
    
    int is_first = 1;
    
    while (num_names > 0) {
        if (!is_first && !ExpString_AppendChar(&str, del)) {
            goto fail1;
        }
        if (!ExpString_Append(&str, names)) {
            goto fail1;
        }
        names += strlen(names) + 1;
        num_names--;
        is_first = 0;
    }
    
    return ExpString_Get(&str);
    
fail1:
    ExpString_Free(&str);
fail0:
    return NULL;
}

#endif
