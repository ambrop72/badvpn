/**
 * @file CStringTrie_header.h
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

// Preprocessor inputs:
// CSTRINGTRIE_PARAM_NAME - name of trie object and prefix for other identifiers
// CSTRINGTRIE_PARAM_VALUE - type of value
// CSTRINGTRIE_PARAM_DEFAULT - default value
// CSTRINGTRIE_PARAM_SIGNIFICANT_BITS - how many low bits of characters are significant

// types
#define CStringTrie CSTRINGTRIE_PARAM_NAME
#define CStringTrieValue CSTRINGTRIE_PARAM_VALUE

// public functions
#define CStringTrie_Init MERGE(CStringTrie, _Init)
#define CStringTrie_Free MERGE(CStringTrie, _Free)
#define CStringTrie_Set MERGE(CStringTrie, _Set)
#define CStringTrie_Get MERGE(CStringTrie, _Get)

// private things
#define CStringTrie__DEGREE (((size_t)1) << CSTRINGTRIE_PARAM_SIGNIFICANT_BITS)
#define CStringTrie__node MERGE(CStringTrie, __node)
#define CStringTrie__new_node MERGE(CStringTrie, __new_node)
