// Copyright (C) 2006 Novell, Inc.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// ============================================================================
// Authors:       
// Contributors:
// Description:    Common provider definitions, macros, etc.
// ============================================================================

#if !defined(__PROVIDER_COMMON_H__)
#define __PROVIDER_COMMON_H__

#include <cmpidt.h>
#include <cmpimacs.h>

#define HOST_INSTRUMENTATION_NS "smash"
#define DEFAULT_NS "root/cimv2"
#define INTEROP_NS "root/cimv2"     /* pegasus likes interop namespace in the default one */

#define UUID_LEN 36
#define XENID_LEN 36 /* Xen UUIDs */

/* Maximum length of system name component in InstanceID properties. */
#define MAX_SYSTEM_NAME_LEN     64

/* Maximum length of InstanceID property. */
#define MAX_INSTANCEID_LEN      128

/* Maximum length of a trace message. */
#define MAX_TRACE_LEN           256

#include "dmtf.h" /* All DMTF values and value maps from the DMTF MOF file are defined here */
#include "cmpitrace.h" /* tracing related helpers */
#include "cmpiutil.h" 

#endif /* __PROVIDER_COMMON_H__ */
