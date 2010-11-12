/**
 * @file FlowError.c
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

#include <flow/FlowError.h>

void FlowErrorDomain_Init (FlowErrorDomain *d, FlowErrorDomain_handler handler, void *user)
{
    d->handler = handler;
    d->user = user;
}

FlowErrorReporter FlowErrorReporter_Create (FlowErrorDomain *domain, int component)
{
    FlowErrorReporter r;
    r.domain = domain;
    r.component = component;
    return r;
}

void FlowErrorReporter_ReportError (FlowErrorReporter *reporter, const void *data)
{
    reporter->domain->handler(reporter->domain->user, reporter->component, data);
}
