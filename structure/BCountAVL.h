/**
 * @file BCountAVL.h
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

#ifndef BADVPN_BCOUNTAVL_H
#define BADVPN_BCOUNTAVL_H

#ifdef BADVPN_STRUCTURE_BAVL_H
#define BADVPN_STRUCTURE_BAVL_H_SAVED BADVPN_STRUCTURE_BAVL_H
#undef BADVPN_STRUCTURE_BAVL_H
#endif

#define BAVL_COUNT

#define BAVL_comparator BCountAVL_comparator
#define BAVL BCountAVL
#define BAVLNode BCountAVLNode
#define BAVL_Init BCountAVL_Init
#define BAVL_Insert BCountAVL_Insert
#define BAVL_Remove BCountAVL_Remove
#define BAVL_IsEmpty BCountAVL_IsEmpty
#define BAVL_Lookup BCountAVL_Lookup
#define BAVL_LookupExact BCountAVL_LookupExact
#define BAVL_GetFirst BCountAVL_GetFirst
#define BAVL_GetLast BCountAVL_GetLast
#define BAVL_GetNext BCountAVL_GetNext
#define BAVL_GetPrev BCountAVL_GetPrev
#define BAVL_Count BCountAVL_Count
#define BAVL_IndexOf BCountAVL_IndexOf
#define BAVL_GetAt BCountAVL_GetAt
#define _BAVL_node_value _BCountAVL_node_value
#define _BAVL_compare_values _BCountAVL_compare_values
#define _BAVL_compare_nodes _BCountAVL_compare_nodes
#define _BAVL_assert_recurser _BCountAVL_assert_recurser
#define _BAVL_assert _BCountAVL_assert
#define _BAVL_update_count_from_children _BCountAVL_update_count_from_children
#define _BAVL_rotate _BCountAVL_rotate
#define _BAVL_subtree_max _BCountAVL_subtree_max
#define _BAVL_replace_subtree _BCountAVL_replace_subtree
#define _BAVL_swap_nodes _BCountAVL_swap_nodes
#define _BAVL_rebalance _BCountAVL_rebalance

#include "BAVL.h"

#undef BAVL_comparator
#undef BAVL
#undef BAVLNode
#undef BAVL_Init
#undef BAVL_Insert
#undef BAVL_Remove
#undef BAVL_IsEmpty
#undef BAVL_Lookup
#undef BAVL_LookupExact
#undef BAVL_GetFirst
#undef BAVL_GetLast
#undef BAVL_GetNext
#undef BAVL_GetPrev
#undef BAVL_Count
#undef BAVL_IndexOf
#undef BAVL_GetAt
#undef _BAVL_node_value
#undef _BAVL_compare_values
#undef _BAVL_compare_nodes
#undef _BAVL_assert_recurser
#undef _BAVL_assert
#undef _BAVL_update_count_from_children
#undef _BAVL_rotate
#undef _BAVL_subtree_max
#undef _BAVL_replace_subtree
#undef _BAVL_swap_nodes
#undef _BAVL_rebalance

#undef BAVL_COUNT

#undef BADVPN_STRUCTURE_BAVL_H

#ifdef BADVPN_STRUCTURE_BAVL_H_SAVED
#define BADVPN_STRUCTURE_BAVL_H BADVPN_STRUCTURE_BAVL_H_SAVED
#undef BADVPN_STRUCTURE_BAVL_H_SAVED
#endif

#endif
