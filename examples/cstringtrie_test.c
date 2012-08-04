#include <stdio.h>

#include <misc/debug.h>
#include <misc/array_length.h>
#include <structure/CStringTrie.h>

#include "cstringtrie_test_trie.h"
#include <structure/CStringTrie_decl.h>

#include "cstringtrie_test_trie.h"
#include <structure/CStringTrie_impl.h>

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
    
    MyTrie trie;
    res = MyTrie_Init(&trie);
    ASSERT_FORCE(res);
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        res = MyTrie_Set(&trie, strings[i], i);
        ASSERT_FORCE(res);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        int value = MyTrie_Get(&trie, strings[i]);
        ASSERT_FORCE(value == i);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        res = MyTrie_Set(&trie, strings[i], NUM_STRINGS - 1 - i);
        ASSERT_FORCE(res);
    }
    
    for (int i = 0; i < NUM_STRINGS; i++) {
        int value = MyTrie_Get(&trie, strings[i]);
        ASSERT_FORCE(value == NUM_STRINGS - 1 - i);
    }
    
    for (int i = 0; i < NUM_OTHER_STRINGS; i++) {
        int value = MyTrie_Get(&trie, other_strings[i]);
        ASSERT_FORCE(value == -1);
    }
    
    MyTrie_Free(&trie);
    return 0;
}
