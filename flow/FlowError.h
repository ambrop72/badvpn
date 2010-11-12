/**
 * @file FlowError.h
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
 * Flow error handling.
 */

#ifndef BADVPN_FLOW_FLOWERROR_H
#define BADVPN_FLOW_FLOWERROR_H

#include <stdint.h>

/**
 * Callback function invoked when {@link FlowErrorReporter_ReportError} is called.
 *
 * @param user value specified to {@link FlowErrorDomain_Init}
 * @param component identifier of the component reporting the error, as in
 *                  {@link FlowErrorReporter_Create}
 * @param data component-specific error data, as in {@link FlowErrorReporter_ReportError}
 */ 
typedef void (*FlowErrorDomain_handler) (void *user, int component, const void *data);

/**
 * Object used to report errors from multiple sources to the same error handler.
 */
typedef struct {
    FlowErrorDomain_handler handler;
    void *user;
} FlowErrorDomain;

/**
 * Initializes the error domain.
 *
 * @param d the object
 * @param handler callback function invoked when {@link FlowErrorReporter_ReportError} is called
 * @param user value passed to callback functions
 */
void FlowErrorDomain_Init (FlowErrorDomain *d, FlowErrorDomain_handler handler, void *user);

/**
 * Structure that can be passed to flow components to ease error reporting.
 */
typedef struct {
    FlowErrorDomain *domain;
    int component;
} FlowErrorReporter;

/**
 * Creates a {@link FlowErrorReporter} structure.
 *
 * @param domain error domain
 * @param component component identifier
 * @return a {@link FlowErrorReporter} structure with the specifed error domain and component.
 */
FlowErrorReporter FlowErrorReporter_Create (FlowErrorDomain *domain, int component);

/**
 * Reports an error.
 *
 * @param reporter a {@link FlowErrorReporter} structure containing the error domain and
 *                 component identifier user to report the error
 * @param data component-specific error data
 */
void FlowErrorReporter_ReportError (FlowErrorReporter *reporter, const void *data);

#endif
