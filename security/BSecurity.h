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
 * Initializes security functions.
 * Security must not be initialized.
 * 
 * @param use_threads whether the application may call security functions
 *                    from different threads. Must be 0 or 1.
 * @return 1 on success, 0 on failure
 */
int BSecurity_GlobalInit (int use_threads);

/**
 * Deinitializes security functions.
 * Security must be initialized.
 */
void BSecurity_GlobalFree (void);

/**
 * Asserts that {@link BSecurity_GlobalInit} was done, and that it was
 * done with use_threads=1 if need_threads=1.
 * 
 * @param need_threads whether the application may call security functions
 *                     from different threads. Must be 0 or 1.
 */
void BSecurity_GlobalAssert (int need_threads);

#endif
