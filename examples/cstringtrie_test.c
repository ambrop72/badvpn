/**
 * @file cstringtrie_test.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
