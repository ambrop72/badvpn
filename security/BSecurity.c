/**
 * @file BSecurity.c
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

#ifdef BADVPN_THREADWORK_USE_PTHREAD
    #include <pthread.h>
#endif

#include <openssl/crypto.h>

#include <misc/debug.h>
#include <misc/balloc.h>

#include <security/BSecurity.h>

int bsecurity_initialized = 0;
int bsecurity_use_threads;

#ifdef BADVPN_THREADWORK_USE_PTHREAD
pthread_mutex_t *bsecurity_locks;
int bsecurity_num_locks;
#endif

#ifdef BADVPN_THREADWORK_USE_PTHREAD

static unsigned long id_callback (void)
{
    ASSERT(bsecurity_initialized)
    ASSERT(bsecurity_use_threads)
    
    return (unsigned long)pthread_self();
}

static void locking_callback (int mode, int type, const char *file, int line)
{
    ASSERT(bsecurity_initialized)
    ASSERT(bsecurity_use_threads)
    ASSERT(type >= 0)
    ASSERT(type < bsecurity_num_locks)
    
    if ((mode & CRYPTO_LOCK)) {
        ASSERT_FORCE(pthread_mutex_lock(&bsecurity_locks[type]) == 0)
    } else {
        ASSERT_FORCE(pthread_mutex_unlock(&bsecurity_locks[type]) == 0)
    }
}

#endif

int BSecurity_GlobalInit (int use_threads)
{
    ASSERT(use_threads == 0 || use_threads == 1)
    ASSERT(!bsecurity_initialized)
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    
    if (use_threads) {
        // get number of locks
        int num_locks = CRYPTO_num_locks();
        ASSERT_FORCE(num_locks >= 0)
        
        // alloc locks array
        if (!(bsecurity_locks = BAllocArray(num_locks, sizeof(bsecurity_locks[0])))) {
            goto fail0;
        }
        
        // init locks
        bsecurity_num_locks = 0;
        for (int i = 0; i < num_locks; i++) {
            if (pthread_mutex_init(&bsecurity_locks[i], NULL) != 0) {
                goto fail1;
            }
            bsecurity_num_locks++;
        }
    }
    
    #endif
    
    bsecurity_initialized = 1;
    bsecurity_use_threads = use_threads;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    if (use_threads) {
        CRYPTO_set_id_callback(id_callback);
        CRYPTO_set_locking_callback(locking_callback);
    }
    #endif
    
    return 1;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
fail1:
    while (bsecurity_num_locks > 0) {
        ASSERT_FORCE(pthread_mutex_destroy(&bsecurity_locks[bsecurity_num_locks - 1]) == 0)
        bsecurity_num_locks--;
    }
    BFree(bsecurity_locks);
fail0:
    return 0;
    #endif
}

void BSecurity_GlobalAssert (int need_threads)
{
    ASSERT(need_threads == 0 || need_threads == 1)
    ASSERT(bsecurity_initialized)
    ASSERT(!(need_threads) || bsecurity_use_threads)
}
