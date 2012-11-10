/**
 * @file NCDRefTarget.h
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

#ifndef BADVPN_NCD_REF_TARGET_H
#define BADVPN_NCD_REF_TARGET_H

#include <misc/debug.h>
#include <base/DebugObject.h>

/**
 * Represents a reference-counted object.
 */
typedef struct NCDRefTarget_s NCDRefTarget;

/**
 * Callback function called after the reference count of a {@link NCDRefTarget}
 * reaches has reached zero. At this point the NCDRefTarget object has already
 * been invalidated, i.e. {@link NCDRefTarget_Ref} must not be called on this
 * object after this handler is called.
 */
typedef void (*NCDRefTarget_func_release) (NCDRefTarget *o);

struct NCDRefTarget_s {
    NCDRefTarget_func_release func_release;
    int refcnt;
    DebugObject d_obj;
};

/**
 * Initializes a reference target object. The initial reference count of the object
 * is 1. The \a func_release argument specifies the function to be called from
 * {@link NCDRefTarget_Deref} when the reference count reaches zero.
 */
void NCDRefTarget_Init (NCDRefTarget *o, NCDRefTarget_func_release func_release);

/**
 * Decrements the reference count of a reference target object. If the reference
 * count has reached zero, the object's {@link NCDRefTarget_func_release} function
 * is called, and the object is considered destroyed.
 */
void NCDRefTarget_Deref (NCDRefTarget *o);

/**
 * Increments the reference count of a reference target object.
 * Returns 1 on success and 0 on failure.
 */
int NCDRefTarget_Ref (NCDRefTarget *o) WARN_UNUSED;

#endif
