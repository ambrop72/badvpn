/**
 * @file CAvl_decl.h
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

typedef struct {
    CAvlLink root;
} CAvl;

typedef struct {
    CAvlEntry *ptr;
    CAvlLink link;
} CAvlNode;

static const CAvlLink CAvlNullLink = CAVL_PARAM_NULL;

static void CAvl_Init (CAvl *o);
static CAvlNode CAvl_Deref (CAvlArg arg, CAvlLink link);
#if !CAVL_PARAM_KEYS_ARE_INDICES
static int CAvl_Insert (CAvl *o, CAvlArg arg, CAvlNode node, CAvlNode *out_ref);
#endif
static void CAvl_Remove (CAvl *o, CAvlArg arg, CAvlNode node);
#if !CAVL_PARAM_KEYS_ARE_INDICES
static CAvlNode CAvl_Lookup (const CAvl *o, CAvlArg arg, CAvlKey key);
static CAvlNode CAvl_LookupExact (const CAvl *o, CAvlArg arg, CAvlKey key);
#endif
static CAvlNode CAvl_GetFirst (const CAvl *o, CAvlArg arg);
static CAvlNode CAvl_GetLast (const CAvl *o, CAvlArg arg);
static CAvlNode CAvl_GetNext (const CAvl *o, CAvlArg arg, CAvlNode node);
static CAvlNode CAvl_GetPrev (const CAvl *o, CAvlArg arg, CAvlNode node);
static int CAvl_IsEmpty (const CAvl *o);
static void CAvl_Verify (const CAvl *o, CAvlArg arg);

#if CAVL_PARAM_USE_COUNTS
static CAvlCount CAvl_Count (const CAvl *o, CAvlArg arg);
static CAvlCount CAvl_IndexOf (const CAvl *o, CAvlArg arg, CAvlNode node);
static CAvlNode CAvl_GetAt (const CAvl *o, CAvlArg arg, CAvlCount index);
#endif

#if CAVL_PARAM_KEYS_ARE_INDICES
static void CAvl_InsertAt (CAvl *o, CAvlArg arg, CAvlNode node, CAvlCount index);
#endif

#include "CAvl_footer.h"
