/**
 * @file CAvl_header.h
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
// CAVL_PARAM_USE_COUNTS - whether to keep node counts (0 or 1)
// CAVL_PARAM_NAME - name of this data structure
// CAVL_PARAM_ENTRY - type of entry
// CAVL_PARAM_LINK - type of node link (usually pointer)
// CAVL_PARAM_KEY - type of key
// CAVL_PARAM_ARG - type of argument pass through to comparisons
// CAVL_PARAM_COUNT - type of count
// CAVL_PARAM_COUNT_MAX - maximum value of count
// CAVL_PARAM_NULL - invalid link
// CAVL_PARAM_DEREF(arg, link) - dereference a non-null link
// CAVL_PARAM_COMPARE_NODES(arg, node1, node2) - compare nodes
// CAVL_PARAM_COMPARE_KEY_NODE(arg, key1, node2) - compare key and node
// CAVL_PARAM_NODE_LINK - link member in node
// CAVL_PARAM_NODE_BALANCE - balance member in node
// CAVL_PARAM_NODE_PARENT - parent member in node
// CAVL_PARAM_NODE_COUNT - count member in node (if CAVL_PARAM_USE_COUNTS)

// types
#define CAvl CAVL_PARAM_NAME
#define CAvlEntry CAVL_PARAM_ENTRY
#define CAvlLink CAVL_PARAM_LINK
#define CAvlNode MERGE(CAvl, Node)
#define CAvlArg CAVL_PARAM_ARG
#define CAvlKey CAVL_PARAM_KEY
#define CAvlCount CAVL_PARAM_COUNT

// static values
#define CAvlNullLink MERGE(CAvl, NullLink)

// public functions
#define CAvl_Init MERGE(CAvl, _Init)
#define CAvl_Deref MERGE(CAvl, _Deref)
#define CAvl_Insert MERGE(CAvl, _Insert)
#define CAvl_Remove MERGE(CAvl, _Remove)
#define CAvl_Lookup MERGE(CAvl, _Lookup)
#define CAvl_LookupExact MERGE(CAvl, _LookupExact)
#define CAvl_GetFirst MERGE(CAvl, _GetFirst)
#define CAvl_GetLast MERGE(CAvl, _GetLast)
#define CAvl_GetNext MERGE(CAvl, _GetNext)
#define CAvl_GetPrev MERGE(CAvl, _GetPrev)
#define CAvl_IsEmpty MERGE(CAvl, _IsEmpty)
#define CAvl_Verify MERGE(CAvl, _Verify)
#define CAvl_Count MERGE(CAvl, _Count)
#define CAvl_IndexOf MERGE(CAvl, _IndexOf)
#define CAvl_GetAt MERGE(CAvl, _GetAt)

// private stuff
#define CAvl_link(node) ((node).ptr->CAVL_PARAM_NODE_LINK)
#define CAvl_balance(node) ((node).ptr->CAVL_PARAM_NODE_BALANCE)
#define CAvl_parent(node) ((node).ptr->CAVL_PARAM_NODE_PARENT)
#define CAvl_count(node) ((node).ptr->CAVL_PARAM_NODE_COUNT)
#define CAvl_nullnode MERGE(CAvl, __nullnode)
#define CAvl_compare_nodes MERGE(CAvl, __compare_nodes)
#define CAvl_compare_key_node MERGE(CAvl, __compare_key_node)
#define CAvl_check_parent MERGE(CAvl, __check_parent)
#define CAvl_verify_recurser MERGE(CAvl, __verify_recurser)
#define CAvl_assert_tree MERGE(CAvl, __assert_tree)
#define CAvl_update_count_from_children MERGE(CAvl, __update_count_from_children)
#define CAvl_rotate MERGE(CAvl, __rotate)
#define CAvl_subtree_min MERGE(CAvl, __subtree_min)
#define CAvl_subtree_max MERGE(CAvl, __subtree_max)
#define CAvl_replace_subtree_fix_counts MERGE(CAvl, __replace_subtree_fix_counts)
#define CAvl_swap_nodes MERGE(CAvl, __swap_nodes)
#define CAvl_rebalance MERGE(CAvl, __rebalance)
#define CAvl_MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define CAvl_OPTNEG(_a, _neg) ((_neg) ? -(_a) : (_a))
