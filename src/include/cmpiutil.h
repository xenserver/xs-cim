// Copyright (C) 2006 IBM Corporation
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
// Author:       Dr. Gareth S. Bestor <bestor@us.ibm.com>
// Contributors:
// Summary:      Some useful utility functions
// Description:
//    TODO
// For more information about the SBLIM Project see:
//    http://sblim.sourceforge.net/
// ============================================================================

#ifndef _CMPIUTIL_H_
#define _CMPIUTIL_H_

/* Include the required CMPI data types. */
#include "cmpidt.h"
#include "stdio.h"
/* Redefine the CMGetCharPtr() macro to better handle NULL strings. */
#ifdef CMGetCharPtr
# undef CMGetCharPtr
# define CMGetCharPtr(s) (((s)==NULL || *((void**)(s))==NULL)? NULL : (char*)((s)->hdl))
#endif

/* Useful shortcut for CMNewInstance(broker, CMNewObjectPath(broker,ns,cn,rc), rc). */
CMPIInstance * _CMNewInstance(const CMPIBroker *mb, char *ns, char *cn, CMPIStatus *rc);

/* Useful boolean constants. */
#ifndef CMPI_true
#define CMPI_true 1
#endif

#ifndef CMPI_false
#define CMPI_false 0
#endif

/* Functions for determining equality of CMPI data types. */
int _CMSameType( CMPIData value1, CMPIData value2 );
int _CMSameValue( CMPIData value1, CMPIData value2 );
int _CMSameObject( CMPIObjectPath * object1, CMPIObjectPath * object2 );
int _CMSameInstance( CMPIInstance * instance1, CMPIInstance * instance2 );

/* Return a string name for all the CMPI return codes. */
const char * _CMPIrcName ( CMPIrc rc );

/* Return a string name for all the CMPI data types. */
const char * _CMPITypeName ( CMPIType type );

/* Return a string representation of a CMPI value. */
char * _CMPIValueToString ( CMPIData data );

/*
 * A function used to extract the key value from a key property.
 */
typedef char * (*_CMPIKeyValExtractFunc_t)(char *, const char *, size_t);

/* Create InstanceID strings of the right form */
int _CMPICreateNewSystemInstanceID(char *buf, int buf_len, char *systemid);
int _CMPICreateNewDeviceInstanceID(char *buf, int buf_len, char *systemid, char *deviceid);
/*
 * Return the 'system' component of InstanceID property values.
 * The system name will be returned in buffer buf of buf_len.
 * Syntax of id parameter
 * Xen:<domain uuid>[\<device uuid>]
 */
char * _CMPIStrncpySystemNameFromID(char *buf, const char *id, size_t buf_len);

/*
 * Return the 'device name' component of InstanceID property values.
 * The device name be returned in buffer buf of buf_len.
 * Syntax of id parameter
 * Xen:<domain name>[\<device name>]
 */
char * _CMPIStrncpyDeviceNameFromID(char *buf, const char *id, size_t buf_len);

#endif /* _CMPIUTIL_H_ */
