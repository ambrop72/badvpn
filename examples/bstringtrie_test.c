#include <stdio.h>

#include <misc/debug.h>
#include <misc/array_length.h>
#include <structure/BStringTrie.h>

const char *strings[] = {
    "hello", "world", "hell", "he", "war", "warning", "warned", "", "heap", "why", "not", "nowhere", "neither",
    "normal", "how", "apple", "apear", "appreciate", "systematic", "systemic", "system", "self", "serious"
};
#define NUM_STRINGS B_ARRAY_LENGTH(strings)

const char *other_strings[] = {
    "warn", "wor", "helloo", "norma", "systems", "server", "no", "when", "nothing"
};
#define NUM_OTHER_STRINGS B_ARRAY_LENGTH(other_strings)

int main ()
{
    int res;
    
    BStringTrie trie;
    res = BStringTrie_Init(&trie);
    ASSERT_FORCE(res);
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        res = BStringTrie_Set(&trie, strings[i], i);
        ASSERT_FORCE(res);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        int value = BStringTrie_Lookup(&trie, strings[i]);
        ASSERT_FORCE(value == i);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        res = BStringTrie_Set(&trie, strings[i], NUM_STRINGS - 1 - i);
        ASSERT_FORCE(res);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        int value = BStringTrie_Lookup(&trie, strings[i]);
        ASSERT_FORCE(value == NUM_STRINGS - 1 - i);
    }
    
    for (int i = 0; i < NUM_OTHER_STRINGS; i++) {
        int value = BStringTrie_Lookup(&trie, other_strings[i]);
        ASSERT_FORCE(value == BSTRINGTRIE_DEFAULT_VALUE);
    }
    
    BStringTrie_Free(&trie);
    return 0;
}
