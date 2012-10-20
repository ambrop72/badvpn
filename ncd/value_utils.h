/**
 * @file value_utils.h
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

#ifndef NCD_VALUE_UTILS_H
#define NCD_VALUE_UTILS_H

#include <stdint.h>

#include <misc/debug.h>
#include <misc/parse_number.h>
#include <ncd/NCDVal.h>
#include <ncd/static_strings.h>

static int ncd_is_none (NCDValRef val);
static NCDValRef ncd_make_boolean (NCDValMem *mem, int value, NCDStringIndex *string_index);
static int ncd_read_boolean (NCDValRef val);
static int ncd_read_uintmax (NCDValRef string, uintmax_t *out) WARN_UNUSED;

static int ncd_is_none (NCDValRef val)
{
    ASSERT(NCDVal_IsString(val))
    
    if (NCDVal_IsIdString(val)) {
        return NCDVal_IdStringId(val) == NCD_STRING_NONE;
    } else {
        return NCDVal_StringEquals(val, "<none>");
    }
}

static NCDValRef ncd_make_boolean (NCDValMem *mem, int value, NCDStringIndex *string_index)
{
    ASSERT(mem)
    ASSERT(string_index)
    
    NCD_string_id_t str_id = (value ? NCD_STRING_TRUE : NCD_STRING_FALSE);
    return NCDVal_NewIdString(mem, str_id, string_index);
}

static int ncd_read_boolean (NCDValRef val)
{
    ASSERT(NCDVal_IsString(val))
    
    if (NCDVal_IsIdString(val)) {
        return NCDVal_IdStringId(val) == NCD_STRING_TRUE;
    } else {
        return NCDVal_StringEquals(val, "true");
    }
}

static int ncd_read_uintmax (NCDValRef string, uintmax_t *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    return parse_unsigned_integer_bin(NCDVal_StringValue(string), NCDVal_StringLength(string), out);
}

#endif
