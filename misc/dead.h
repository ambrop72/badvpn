/**
 * @file dead.h
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
 * Dead mechanism definitions.
 * 
 * The dead mechanism is a way for a piece of code to detect whether some
 * specific event has occured during some operation (usually during calling
 * a user-provided handler function), without requiring access to memory
 * that might no longer be available because of the event.
 * 
 * It works somehow like that:
 * 
 * First a dead variable ({@link dead_t}) is allocated somewhere, and
 * initialized with {@link DEAD_INIT}, e.g.:
 *   DEAD_INIT(dead);
 * 
 * When the event that needs to be caught occurs, {@link DEAD_KILL} is
 * called, e.g.:
 *   DEAD_KILL(dead);
 * The memory used by the dead variable is no longer needed after
 * that.
 * 
 * If a piece of code needs to know whether the event occured during some
 * operation (but it must not have occured before!), it puts {@link DEAD_ENTER}}
 * in front of the operation, and does {@link DEAD_LEAVE} at the end. If
 * {@link DEAD_LEAVE} returned nonzero, the event occured, otherwise it did
 * not. Example:
 *   DEAD_ENTER(dead)
 *   HandlerFunction();
 *   if (DEAD_LEAVE(dead)) {
 *     (event occured)
 *   }
 * 
 * If is is needed to check for the event more than once in a single block,
 * {@link DEAD_DECLARE} should be put somewhere before, and {@link DEAD_ENTER2}
 * should be used instead of {@link DEAD_ENTER}.
 * 
 * If it is needed to check for multiple events (dead variables) at the same
 * time, DEAD_*_N macros should be used instead, specifying different
 * identiers as the first argument for different dead variables.
 */

#ifndef BADVPN_MISC_DEAD_H
#define BADVPN_MISC_DEAD_H

#include <stdlib.h>

/**
 * Dead variable.
 */
typedef int *dead_t;

/**
 * Initializes a dead variable.
 */
#define DEAD_INIT(ptr) { ptr = NULL; }

/**
 * Kills the dead variable,
 */
#define DEAD_KILL(ptr) { if (ptr) *(ptr) = 1; }

/**
 * Kills the dead variable with the given value, or does nothing
 * if the value is 0. The value will seen by {@link DEAD_KILLED}.
 */
#define DEAD_KILL_WITH(ptr, val) { if (ptr) *(ptr) = (val); }

/**
 * Declares dead catching variables.
 */
#define DEAD_DECLARE int __dead; dead_t __prev_ptr;

/**
 * Enters a dead catching using already declared dead catching variables.
 * The dead variable must have been initialized with {@link DEAD_INIT},
 * and {@link DEAD_KILL} must not have been called yet.
 * {@link DEAD_LEAVE2} must be called before the current scope is left.
 */
#define DEAD_ENTER2(ptr) { __dead = 0; __prev_ptr = ptr; ptr = &__dead; }

/**
 * Declares dead catching variables and enters a dead catching.
 * The dead variable must have been initialized with {@link DEAD_INIT},
 * and {@link DEAD_KILL} must not have been called yet.
 * {@link DEAD_LEAVE2} must be called before the current scope is left.
 */
#define DEAD_ENTER(ptr) DEAD_DECLARE DEAD_ENTER2(ptr)

/**
 * Leaves a dead catching.
 */
#define DEAD_LEAVE2(ptr) { if (!__dead) ptr = __prev_ptr; if (__prev_ptr) *__prev_ptr = __dead; }

/**
 * Returns 1 if {@link DEAD_KILL} was called for the dead variable, 0 otherwise.
 * Must be called after entering a dead catching.
 */
#define DEAD_KILLED (__dead)

#define DEAD_DECLARE_N(n) int __dead##n; dead_t __prev_ptr##n;
#define DEAD_ENTER2_N(n, ptr) { __dead##n = 0; __prev_ptr##n = ptr; ptr = &__dead##n;}
#define DEAD_ENTER_N(n, ptr) DEAD_DECLARE_N(n) DEAD_ENTER2_N(n, ptr)
#define DEAD_LEAVE2_N(n, ptr) { if (!__dead##n) ptr = __prev_ptr##n; if (__prev_ptr##n) *__prev_ptr##n = __dead##n; }
#define DEAD_KILLED_N(n) (__dead##n)

#endif
