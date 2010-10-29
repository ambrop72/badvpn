/**
 * @file OTPGenerator.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <security/OTPGenerator.h>

int OTPGenerator_Init (OTPGenerator *g, int num_otps, int cipher)
{
    ASSERT(num_otps >= 0)
    ASSERT(BEncryption_cipher_valid(cipher))
    
    // init arguments
    g->num_otps = num_otps;
    
    // init position
    g->position = g->num_otps;
    
    // init calculator
    if (!OTPCalculator_Init(&g->calc, g->num_otps, cipher)) {
        goto fail0;
    }
    
    // init debug object
    DebugObject_Init(&g->d_obj);

    return 1;
    
fail0:
    return 0;
}

void OTPGenerator_Free (OTPGenerator *g)
{
    // free debug object
    DebugObject_Free(&g->d_obj);
    
    // free calculator
    OTPCalculator_Free(&g->calc);
}

void OTPGenerator_SetSeed (OTPGenerator *g, uint8_t *key, uint8_t *iv)
{
    g->otps = OTPCalculator_Generate(&g->calc, key, iv, 1);
    g->position = 0;
}

int OTPGenerator_GetPosition (OTPGenerator *g)
{
    return g->position;
}

void OTPGenerator_Reset (OTPGenerator *g)
{
    g->position = g->num_otps;
}

otp_t OTPGenerator_GetOTP (OTPGenerator *g)
{
    ASSERT(g->position < g->num_otps)
    
    return g->otps[g->position++];
}
