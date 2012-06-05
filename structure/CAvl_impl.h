/**
 * @file CAvl_impl.h
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

#include "CAvl_header.h"

static CAvlNode CAvl_nullnode (void)
{
    CAvlNode n;
    n.link = CAvlNullLink;
    n.ptr = NULL;
    return n;
}

static int CAvl_compare_nodes (CAvlArg arg, CAvlNode node1, CAvlNode node2)
{
    int res = CAVL_PARAM_COMPARE_NODES(arg, node1, node2);
    ASSERT(res >= -1)
    ASSERT(res <= 1)
    
    return res;
}

static int CAvl_compare_key_node (CAvlArg arg, CAvlKey key1, CAvlNode node2)
{
    int res = CAVL_PARAM_COMPARE_KEY_NODE(arg, key1, node2);
    ASSERT(res >= -1)
    ASSERT(res <= 1)
    
    return res;
}

static int CAvl_check_parent (CAvlNode p, CAvlNode c)
{
    return (p.link == CAvl_parent(c)) && (p.link == CAvlNullLink || c.link == CAvl_link(p)[0] || c.link == CAvl_link(p)[1]);
}

static int CAvl_verify_recurser (CAvlArg arg, CAvlNode n)
{
    ASSERT_FORCE(CAvl_balance(n) >= -1)
    ASSERT_FORCE(CAvl_balance(n) <= 1)
    
    int height_left = 0;
    int height_right = 0;
#if CAVL_PARAM_USE_COUNTS
    CAvlCount count_left = 0;
    CAvlCount count_right = 0;
#endif
    
    // check left subtree
    if (CAvl_link(n)[0] != CAvlNullLink) {
        // check parent link
        ASSERT_FORCE(CAvl_parent(CAvl_Deref(arg, CAvl_link(n)[0])) == n.link)
        // check binary search tree
        ASSERT_FORCE(CAvl_compare_nodes(arg, CAvl_Deref(arg, CAvl_link(n)[0]), n) == -1)
        // recursively calculate height
        height_left = CAvl_verify_recurser(arg, CAvl_Deref(arg, CAvl_link(n)[0]));
#if CAVL_PARAM_USE_COUNTS
        count_left = CAvl_count(CAvl_Deref(arg, CAvl_link(n)[0]));
#endif
    }
    
    // check right subtree
    if (CAvl_link(n)[1] != CAvlNullLink) {
        // check parent link
        ASSERT_FORCE(CAvl_parent(CAvl_Deref(arg, CAvl_link(n)[1])) == n.link)
        // check binary search tree
        ASSERT_FORCE(CAvl_compare_nodes(arg, CAvl_Deref(arg, CAvl_link(n)[1]), n) == 1)
        // recursively calculate height
        height_right = CAvl_verify_recurser(arg, CAvl_Deref(arg, CAvl_link(n)[1]));
#if CAVL_PARAM_USE_COUNTS
        count_right = CAvl_count(CAvl_Deref(arg, CAvl_link(n)[1]));
#endif
    }
    
    // check balance factor
    ASSERT_FORCE(CAvl_balance(n) == height_right - height_left)
    
#if CAVL_PARAM_USE_COUNTS
    // check count
    ASSERT(CAvl_count(n) == 1 + count_left + count_right)
#endif
    
    return CAvl_MAX(height_left, height_right) + 1;
}

static void CAvl_assert_tree (CAvl *o, CAvlArg arg)
{
#ifdef CAVL_PARAM_VERIFY
    CAvl_Verify(o, arg);
#endif
}

#if CAVL_PARAM_USE_COUNTS
static void CAvl_update_count_from_children (CAvlArg arg, CAvlNode n)
{
    CAvlCount left_count = CAvl_link(n)[0] != CAvlNullLink ? CAvl_count(CAvl_Deref(arg, CAvl_link(n)[0])) : 0;
    CAvlCount right_count = CAvl_link(n)[1] != CAvlNullLink ? CAvl_count(CAvl_Deref(arg, CAvl_link(n)[1])) : 0;
    CAvl_count(n) = 1 + left_count + right_count;
}
#endif

static void CAvl_rotate (CAvl *o, CAvlArg arg, CAvlNode r, uint8_t dir, CAvlNode r_parent)
{
    ASSERT(CAvl_check_parent(r_parent, r))
    CAvlNode nr = CAvl_Deref(arg, CAvl_link(r)[!dir]);
    
    CAvl_link(r)[!dir] = CAvl_link(nr)[dir];
    if (CAvl_link(r)[!dir] != CAvlNullLink) {
        CAvl_parent(CAvl_Deref(arg, CAvl_link(r)[!dir])) = r.link;
    }
    CAvl_link(nr)[dir] = r.link;
    CAvl_parent(nr) = r_parent.link;
    if (r_parent.link != CAvlNullLink) {
        CAvl_link(r_parent)[r.link == CAvl_link(r_parent)[1]] = nr.link;
    } else {
        o->root = nr.link;
    }
    CAvl_parent(r) = nr.link;
    
#if CAVL_PARAM_USE_COUNTS
    CAvl_update_count_from_children(arg, r);
    CAvl_update_count_from_children(arg, nr);
#endif
}

static CAvlNode CAvl_subtree_min (CAvlArg arg, CAvlNode n)
{
    ASSERT(n.link != CAvlNullLink)
    
    while (CAvl_link(n)[0] != CAvlNullLink) {
        n = CAvl_Deref(arg, CAvl_link(n)[0]);
    }
    
    return n;
}

static CAvlNode CAvl_subtree_max (CAvlArg arg, CAvlNode n)
{
    ASSERT(n.link != CAvlNullLink)
    
    while (CAvl_link(n)[1] != CAvlNullLink) {
        n = CAvl_Deref(arg, CAvl_link(n)[1]);
    }
    
    return n;
}

static void CAvl_replace_subtree_fix_counts (CAvl *o, CAvlArg arg, CAvlNode dest, CAvlNode n, CAvlNode dest_parent)
{
    ASSERT(dest.link != CAvlNullLink)
    ASSERT(CAvl_check_parent(dest_parent, dest))
    
    if (dest_parent.link != CAvlNullLink) {
        CAvl_link(dest_parent)[dest.link == CAvl_link(dest_parent)[1]] = n.link;
    } else {
        o->root = n.link;
    }
    if (n.link != CAvlNullLink) {
        CAvl_parent(n) = CAvl_parent(dest);
    }
    
#if CAVL_PARAM_USE_COUNTS
    for (CAvlNode c = dest_parent; c.link != CAvlNullLink; c = CAvl_Deref(arg, CAvl_parent(c))) {
        ASSERT(CAvl_count(c) >= CAvl_count(dest))
        CAvl_count(c) -= CAvl_count(dest);
        if (n.link != CAvlNullLink) {
            ASSERT(CAvl_count(n) <= CAVL_PARAM_COUNT_MAX - CAvl_count(c))
            CAvl_count(c) += CAvl_count(n);
        }
    }
#endif
}

static void CAvl_swap_nodes (CAvl *o, CAvlArg arg, CAvlNode n1, CAvlNode n2, CAvlNode n1_parent, CAvlNode n2_parent)
{
    ASSERT(CAvl_check_parent(n1_parent, n1))
    ASSERT(CAvl_check_parent(n2_parent, n2))
    
    if (n2_parent.link == n1.link || n1_parent.link == n2.link) {
        // when the nodes are directly connected we need special handling
        // make sure n1 is above n2
        if (n1_parent.link == n2.link) {
            CAvlNode t = n1;
            n1 = n2;
            n2 = t;
            t = n1_parent;
            n1_parent = n2_parent;
            n2_parent = t;
        }
        
        uint8_t side = (n2.link == CAvl_link(n1)[1]);
        CAvlNode c = CAvl_Deref(arg, CAvl_link(n1)[!side]);
        
        if ((CAvl_link(n1)[0] = CAvl_link(n2)[0]) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n1)[0])) = n1.link;
        }
        if ((CAvl_link(n1)[1] = CAvl_link(n2)[1]) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n1)[1])) = n1.link;
        }
        
        CAvl_parent(n2) = CAvl_parent(n1);
        if (n1_parent.link != CAvlNullLink) {
            CAvl_link(n1_parent)[n1.link == CAvl_link(n1_parent)[1]] = n2.link;
        } else {
            o->root = n2.link;
        }
        
        CAvl_link(n2)[side] = n1.link;
        CAvl_parent(n1) = n2.link;
        if ((CAvl_link(n2)[!side] = c.link) != CAvlNullLink) {
            CAvl_parent(c) = n2.link;
        }
    } else {
        CAvlNode temp;
        
        // swap parents
        temp = n1_parent;
        CAvl_parent(n1) = CAvl_parent(n2);
        if (n2_parent.link != CAvlNullLink) {
            CAvl_link(n2_parent)[n2.link == CAvl_link(n2_parent)[1]] = n1.link;
        } else {
            o->root = n1.link;
        }
        CAvl_parent(n2) = temp.link;
        if (temp.link != CAvlNullLink) {
            CAvl_link(temp)[n1.link == CAvl_link(temp)[1]] = n2.link;
        } else {
            o->root = n2.link;
        }
        
        // swap left children
        temp = CAvl_Deref(arg, CAvl_link(n1)[0]);
        if ((CAvl_link(n1)[0] = CAvl_link(n2)[0]) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n1)[0])) = n1.link;
        }
        if ((CAvl_link(n2)[0] = temp.link) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n2)[0])) = n2.link;
        }
        
        // swap right children
        temp = CAvl_Deref(arg, CAvl_link(n1)[1]);
        if ((CAvl_link(n1)[1] = CAvl_link(n2)[1]) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n1)[1])) = n1.link;
        }
        if ((CAvl_link(n2)[1] = temp.link) != CAvlNullLink) {
            CAvl_parent(CAvl_Deref(arg, CAvl_link(n2)[1])) = n2.link;
        }
    }
    
    // swap balance factors
    int8_t b = CAvl_balance(n1);
    CAvl_balance(n1) = CAvl_balance(n2);
    CAvl_balance(n2) = b;
    
#if CAVL_PARAM_USE_COUNTS
    // swap counts
    CAvlCount c = CAvl_count(n1);
    CAvl_count(n1) = CAvl_count(n2);
    CAvl_count(n2) = c;
#endif
}

static void CAvl_rebalance (CAvl *o, CAvlArg arg, CAvlNode node, uint8_t side, int8_t deltac)
{
    ASSERT(side == 0 || side == 1)
    ASSERT(deltac >= -1 && deltac <= 1)
    ASSERT(CAvl_balance(node) >= -1 && CAvl_balance(node) <= 1)
    
    // if no subtree changed its height, no more rebalancing is needed
    if (deltac == 0) {
        return;
    }
    
    // calculate how much our height changed
    int8_t delta = CAvl_MAX(deltac, CAvl_OPTNEG(CAvl_balance(node), side)) - CAvl_MAX(0, CAvl_OPTNEG(CAvl_balance(node), side));
    ASSERT(delta >= -1 && delta <= 1)
    
    // update our balance factor
    CAvl_balance(node) -= CAvl_OPTNEG(deltac, side);
    
    CAvlNode child;
    CAvlNode gchild;
    
    // perform transformations if the balance factor is wrong
    if (CAvl_balance(node) == 2 || CAvl_balance(node) == -2) {
        uint8_t bside;
        int8_t bsidef;
        if (CAvl_balance(node) == 2) {
            bside = 1;
            bsidef = 1;
        } else {
            bside = 0;
            bsidef = -1;
        }
        
        ASSERT(CAvl_link(node)[bside] != CAvlNullLink)
        child = CAvl_Deref(arg, CAvl_link(node)[bside]);
        
        switch (CAvl_balance(child) * bsidef) {
            case 1:
                CAvl_rotate(o, arg, node, !bside, CAvl_Deref(arg, CAvl_parent(node)));
                CAvl_balance(node) = 0;
                CAvl_balance(child) = 0;
                node = child;
                delta -= 1;
                break;
            case 0:
                CAvl_rotate(o, arg, node, !bside, CAvl_Deref(arg, CAvl_parent(node)));
                CAvl_balance(node) = 1 * bsidef;
                CAvl_balance(child) = -1 * bsidef;
                node = child;
                break;
            case -1:
                ASSERT(CAvl_link(child)[!bside] != CAvlNullLink)
                gchild = CAvl_Deref(arg, CAvl_link(child)[!bside]);
                CAvl_rotate(o, arg, child, bside, node);
                CAvl_rotate(o, arg, node, !bside, CAvl_Deref(arg, CAvl_parent(node)));
                CAvl_balance(node) = -CAvl_MAX(0, CAvl_balance(gchild) * bsidef) * bsidef;
                CAvl_balance(child) = CAvl_MAX(0, -CAvl_balance(gchild) * bsidef) * bsidef;
                CAvl_balance(gchild) = 0;
                node = gchild;
                delta -= 1;
                break;
            default:
                ASSERT(0);
        }
    }
    
    ASSERT(delta >= -1 && delta <= 1)
    // Transformations above preserve this. Proof:
    //     - if a child subtree gained 1 height and rebalancing was needed,
    //       it was the heavier subtree. Then delta was was originally 1, because
    //       the heaviest subtree gained one height. If the transformation reduces
    //       delta by one, it becomes 0.
    //     - if a child subtree lost 1 height and rebalancing was needed, it
    //       was the lighter subtree. Then delta was originally 0, because
    //       the height of the heaviest subtree was unchanged. If the transformation
    //       reduces delta by one, it becomes -1.
    
    if (CAvl_parent(node) != CAvlNullLink) {
        CAvlNode node_parent = CAvl_Deref(arg, CAvl_parent(node));
        CAvl_rebalance(o, arg, node_parent, node.link == CAvl_link(node_parent)[1], delta);
    }
}

static void CAvl_Init (CAvl *o)
{
    o->root = CAvlNullLink;
}

static CAvlNode CAvl_Deref (CAvlArg arg, CAvlLink link)
{
    if (link == CAvlNullLink) {
        return CAvl_nullnode();
    }
    
    CAvlNode n;
    n.ptr = CAVL_PARAM_DEREF(arg, link);
    n.link = link;
    
    ASSERT(n.ptr)
    
    return n;
}

static int CAvl_Insert (CAvl *o, CAvlArg arg, CAvlNode node, CAvlNode *out_ref)
{
    ASSERT(node.link != CAvlNullLink)
    
    // insert to root?
    if (o->root == CAvlNullLink) {
        o->root = node.link;
        CAvl_parent(node) = CAvlNullLink;
        CAvl_link(node)[0] = CAvlNullLink;
        CAvl_link(node)[1] = CAvlNullLink;
        CAvl_balance(node) = 0;
#if CAVL_PARAM_USE_COUNTS
        CAvl_count(node) = 1;
#endif
        
        CAvl_assert_tree(o, arg);
        
        if (out_ref) {
            *out_ref = CAvl_nullnode();
        }
        return 1;
    }
    
    CAvlNode c = CAvl_Deref(arg, o->root);
    int side;
    while (1) {
        int comp = CAvl_compare_nodes(arg, node, c);
        
        if (comp == 0) {
            if (out_ref) {
                *out_ref = c;
            }
            return 0;
        }
        
        side = (comp == 1);
        
        if (CAvl_link(c)[side] == CAvlNullLink) {
            break;
        }
        
        c = CAvl_Deref(arg, CAvl_link(c)[side]);
    }
    
    CAvl_link(c)[side] = node.link;
    CAvl_parent(node) = c.link;
    CAvl_link(node)[0] = CAvlNullLink;
    CAvl_link(node)[1] = CAvlNullLink;
    CAvl_balance(node) = 0;
#if CAVL_PARAM_USE_COUNTS
    CAvl_count(node) = 1;
#endif
    
#if CAVL_PARAM_USE_COUNTS
    for (CAvlNode p = c; p.link != CAvlNullLink; p = CAvl_Deref(arg, CAvl_parent(p))) {
        CAvl_count(p)++;
    }
#endif
    
    CAvl_rebalance(o, arg, c, side, 1);
    
    CAvl_assert_tree(o, arg);
    
    if (out_ref) {
        *out_ref = c;
    }
    return 1;
}

static void CAvl_Remove (CAvl *o, CAvlArg arg, CAvlNode node)
{
    ASSERT(node.link != CAvlNullLink)
    ASSERT(o->root != CAvlNullLink)
    
    if (CAvl_link(node)[0] != CAvlNullLink && CAvl_link(node)[1] != CAvlNullLink) {
        CAvlNode max = CAvl_subtree_max(arg, CAvl_Deref(arg, CAvl_link(node)[0]));
        CAvl_swap_nodes(o, arg, node, max, CAvl_Deref(arg, CAvl_parent(node)), CAvl_Deref(arg, CAvl_parent(max)));
    }
    
    ASSERT(CAvl_link(node)[0] == CAvlNullLink || CAvl_link(node)[1] == CAvlNullLink)
    
    CAvlNode paren = CAvl_Deref(arg, CAvl_parent(node));
    CAvlNode child = (CAvl_link(node)[0] != CAvlNullLink ? CAvl_Deref(arg, CAvl_link(node)[0]) : CAvl_Deref(arg, CAvl_link(node)[1]));
    
    if (paren.link != CAvlNullLink) {
        int side = (node.link == CAvl_link(paren)[1]);
        CAvl_replace_subtree_fix_counts(o, arg, node, child, paren);
        CAvl_rebalance(o, arg, paren, side, -1);
    } else {
        CAvl_replace_subtree_fix_counts(o, arg, node, child, paren);
    }
    
    CAvl_assert_tree(o, arg);
}

static CAvlNode CAvl_Lookup (const CAvl *o, CAvlArg arg, CAvlKey key)
{
    if (o->root == CAvlNullLink) {
        return CAvl_nullnode();
    }
    
    CAvlNode c = CAvl_Deref(arg, o->root);
    while (1) {
        // compare
        int comp = CAvl_compare_key_node(arg, key, c);
        
        // have we found a node that compares equal?
        if (comp == 0) {
            return c;
        }
        
        int side = (comp == 1);
        
        // have we reached a leaf?
        if (CAvl_link(c)[side] == CAvlNullLink) {
            return c;
        }
        
        c = CAvl_Deref(arg, CAvl_link(c)[side]);
    }
}

static CAvlNode CAvl_LookupExact (const CAvl *o, CAvlArg arg, CAvlKey key)
{
    if (o->root == CAvlNullLink) {
        return CAvl_nullnode();
    }
    
    CAvlNode c = CAvl_Deref(arg, o->root);
    while (1) {
        // compare
        int comp = CAvl_compare_key_node(arg, key, c);
        
        // have we found a node that compares equal?
        if (comp == 0) {
            return c;
        }
        
        int side = (comp == 1);
        
        // have we reached a leaf?
        if (CAvl_link(c)[side] == CAvlNullLink) {
            return CAvl_nullnode();
        }
        
        c = CAvl_Deref(arg, CAvl_link(c)[side]);
    }
}

static CAvlNode CAvl_GetFirst (const CAvl *o, CAvlArg arg)
{
    if (o->root == CAvlNullLink) {
        return CAvl_nullnode();
    }
    
    return CAvl_subtree_min(arg, CAvl_Deref(arg, o->root));
}

static CAvlNode CAvl_GetLast (const CAvl *o, CAvlArg arg)
{
    if (o->root == CAvlNullLink) {
        return CAvl_nullnode();
    }
    
    return CAvl_subtree_max(arg, CAvl_Deref(arg, o->root));
}

static CAvlNode CAvl_GetNext (const CAvl *o, CAvlArg arg, CAvlNode node)
{
    ASSERT(node.link != CAvlNullLink)
    ASSERT(o->root != CAvlNullLink)
    
    if (CAvl_link(node)[1] != CAvlNullLink) {
        node = CAvl_Deref(arg, CAvl_link(node)[1]);
        while (CAvl_link(node)[0] != CAvlNullLink) {
            node = CAvl_Deref(arg, CAvl_link(node)[0]);
        }
    } else {
        while (CAvl_parent(node) != CAvlNullLink && node.link == CAvl_link(CAvl_Deref(arg, CAvl_parent(node)))[1]) {
            node = CAvl_Deref(arg, CAvl_parent(node));
        }
        node = CAvl_Deref(arg, CAvl_parent(node));
    }
    
    return node;
}

static CAvlNode CAvl_GetPrev (const CAvl *o, CAvlArg arg, CAvlNode node)
{
    ASSERT(node.link != CAvlNullLink)
    ASSERT(o->root != CAvlNullLink)
    
    if (CAvl_link(node)[0] != CAvlNullLink) {
        node = CAvl_Deref(arg, CAvl_link(node)[0]);
        while (CAvl_link(node)[1] != CAvlNullLink) {
            node = CAvl_Deref(arg, CAvl_link(node)[1]);
        }
    } else {
        while (CAvl_parent(node) != CAvlNullLink && node.link == CAvl_link(CAvl_Deref(arg, CAvl_parent(node)))[0]) {
            node = CAvl_Deref(arg, CAvl_parent(node));
        }
        node = CAvl_Deref(arg, CAvl_parent(node));
    }
    
    return node;
}

static int CAvl_IsEmpty (const CAvl *o)
{
    return o->root == CAvlNullLink;
}

static void CAvl_Verify (const CAvl *o, CAvlArg arg)
{
    if (o->root != CAvlNullLink) {
        CAvlNode root = CAvl_Deref(arg, o->root);
        ASSERT(CAvl_parent(root) == CAvlNullLink)
        CAvl_verify_recurser(arg, root);
    }
}

#if CAVL_PARAM_USE_COUNTS

static CAvlCount CAvl_Count (const CAvl *o, CAvlArg arg)
{
    return (o->root != CAvlNullLink ? CAvl_count(CAvl_Deref(arg, o->root)) : 0);
}

static CAvlCount CAvl_IndexOf (const CAvl *o, CAvlArg arg, CAvlNode node)
{
    ASSERT(node.link != CAvlNullLink)
    ASSERT(o->root != CAvlNullLink)
    
    CAvlCount index = (CAvl_link(node)[0] != CAvlNullLink ? CAvl_count(CAvl_Deref(arg, CAvl_link(node)[0])) : 0);
    
    CAvlNode paren = CAvl_Deref(arg, CAvl_parent(node));
    
    for (CAvlNode c = node; paren.link != CAvlNullLink; c = paren) {
        if (c.link == CAvl_link(paren)[1]) {
            ASSERT(CAvl_count(paren) > CAvl_count(c))
            ASSERT(CAvl_count(paren) - CAvl_count(c) <= CAVL_PARAM_COUNT_MAX - index)
            index += CAvl_count(paren) - CAvl_count(c);
        }
        
        paren = CAvl_Deref(arg, CAvl_parent(c));
    }
    
    return index;
}

static CAvlNode CAvl_GetAt (const CAvl *o, CAvlArg arg, CAvlCount index)
{
    if (index >= CAvl_Count(o, arg)) {
        return CAvl_nullnode();
    }
    
    CAvlNode c = CAvl_Deref(arg, o->root);
    
    while (1) {
        ASSERT(c.link != CAvlNullLink)
        ASSERT(index < CAvl_count(c))
        
        CAvlCount left_count = (CAvl_link(c)[0] != CAvlNullLink ? CAvl_count(CAvl_Deref(arg, CAvl_link(c)[0])) : 0);
        
        if (index == left_count) {
            return c;
        }
        
        if (index < left_count) {
            c = CAvl_Deref(arg, CAvl_link(c)[0]);
        } else {
            c = CAvl_Deref(arg, CAvl_link(c)[1]);
            index -= left_count + 1;
        }
    }
}

#endif

#include "CAvl_footer.h"
