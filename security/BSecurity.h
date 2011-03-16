/**
 * @file BSecurity.h
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
 * 
 * @section DESCRIPTION
 * 
 * Initialization of OpenSSL for security functions.
 */

#ifndef BADVPN_SECURITY_BSECURITY_H
#define BADVPN_SECURITY_BSECURITY_H

/**
 * Initializes thread safety for security functions.
 * Thread safety must not be initialized.
 * 
 * @return 1 on success, 0 on failure
 */
int BSecurity_GlobalInitThreadSafe (void);

/**
 * Deinitializes thread safety for security functions.
 * Thread safety must be initialized.
 */
void BSecurity_GlobalFreeThreadSafe (void);

/**
 * Asserts that {@link BSecurity_GlobalInitThreadSafe} was done,
 * if thread_safe=1.
 * 
 * @param thread_safe whether thread safety is to be asserted.
 *                    Must be 0 or 1.
 */
void BSecurity_GlobalAssertThreadSafe (int thread_safe);

#endif
