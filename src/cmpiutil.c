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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Include the required CMPI data types, function headers, and macros. */
#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"
#include "cmpiutil.h"

/* ------------------------------------------------------------------------- */

/* CMNewInstance() substitute that does the prerequisite CMNewObjectPath() for you. */
CMPIInstance * _CMNewInstance(const CMPIBroker *mb, char *ns, char *cn, CMPIStatus *rc)
{
   CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations. */

   /* First attempt to create a CMPIObjectPath for the new instance. */
   CMPIObjectPath * objectpath = CMNewObjectPath(mb, ns, cn, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) return NULL;
   
   /* Next attempt to create the CMPIInstance. */
   CMPIInstance * instance = CMNewInstance(mb, objectpath, rc);
   if (((rc != NULL) && (rc->rc != CMPI_RC_OK)) || CMIsNullObject(instance)) return NULL;

   /* Bad object path could result from creating a new instance of bad classname */
   CMPIObjectPath *op = CMGetObjectPath(instance, rc);
   if(op == NULL || (rc && rc->rc != CMPI_RC_OK)) return NULL;

   /* Everything worked OK. */
   return instance;
}

/* ------------------------------------------------------------------------- */

/* Compare two CIM types to see if they are identical. */
int _CMSameType( CMPIData value1, CMPIData value2 )
{
   return (value1.type == value2.type);
}

/* ------------------------------------------------------------------------- */

/* Compare two CIM data values to see if they are identical. */
int _CMSameValue( CMPIData value1, CMPIData value2 )
{
   /* Check if the type of the two CIM values is the same. */
   if (!_CMSameType(value1, value2)) return 0;

   /* Check if the value of the two CIM values is the same. */
   switch (value1.type) {
      case CMPI_string: {
         if (CMIsNullObject(value1.value.string) || CMIsNullObject(value2.value.string)) return 0;

         /* Compare the C strings for equality. */
         return (strcmp(CMGetCharPtr(value1.value.string), CMGetCharPtr(value2.value.string)) == 0);
      }

      case CMPI_dateTime: {
         CMPIUint64 dateTime1, dateTime2;	 /* Binary representation of the dateTimes. */
         CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations. */

         if (CMIsNullObject(value1.value.dateTime) || CMIsNullObject(value2.value.dateTime)) return 0;
         dateTime1 = CMGetBinaryFormat(value1.value.dateTime, &status);
         if (status.rc != CMPI_RC_OK) return 0;
         dateTime2 = CMGetBinaryFormat(value2.value.dateTime, &status);
         if (status.rc != CMPI_RC_OK) return 0;

         /* Compare the binary dateTimes for equality. */
         return (dateTime1 == dateTime2);
      }

      /* Compare the simple types for equality. */
      case CMPI_boolean:  return (value1.value.boolean == value2.value.boolean);
      case CMPI_char16:   return (value1.value.char16 == value2.value.char16);
      case CMPI_uint8:    return (value1.value.uint8 == value2.value.uint8);
      case CMPI_sint8:    return (value1.value.sint8 == value2.value.sint8);
      case CMPI_uint16:   return (value1.value.uint16 == value2.value.uint16);
      case CMPI_sint16:   return (value1.value.sint16 == value2.value.sint16);
      case CMPI_uint32:   return (value1.value.uint32 == value2.value.uint32);
      case CMPI_sint32:   return (value1.value.sint32 == value2.value.sint32);
      case CMPI_uint64:   return (value1.value.uint64 == value2.value.uint64);
      case CMPI_sint64:   return (value1.value.sint64 == value2.value.sint64);
      case CMPI_real32:   return (value1.value.real32 == value2.value.real32);
      case CMPI_real64:   return (value1.value.real64 == value2.value.real64);
   }
   return 0; 
}

/* ------------------------------------------------------------------------- */

/* Compare two CIM object paths to see if they are identical. */
int _CMSameObject( CMPIObjectPath * object1, CMPIObjectPath * object2 )
{
   CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations. */
   int i;

   /* Check if the two object paths have the same namespace. */
   CMPIString * namespace1 = CMGetNameSpace(object1, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(namespace1)) return 0;
   CMPIString * namespace2 = CMGetNameSpace(object2, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(namespace2)) return 0;
   if (strcmp(CMGetCharPtr(namespace1), CMGetCharPtr(namespace2)) != 0) return 0;

   /* Check if the two object paths have the same class. */
   CMPIString * classname1 = CMGetClassName(object1, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(classname1)) return 0;
   CMPIString * classname2 = CMGetClassName(object2, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(classname2)) return 0;
   if (strcmp(CMGetCharPtr(classname1), CMGetCharPtr(classname2)) != 0) return 0;
   
   /* Check if the two object paths have the same number of keys. */
   int numkeys1 = CMGetKeyCount(object1, &status);
   if (status.rc != CMPI_RC_OK) return 0;
   int numkeys2 = CMGetKeyCount(object2, &status);
   if (status.rc != CMPI_RC_OK) return 0;
   if (numkeys1 != numkeys2) return 0;

   /* Go through the keys for the 1st object path and compare to the 2nd object path. */
   for (i=0; i<numkeys1; i++) {
      CMPIString * keyname = NULL;

      /* Retrieve the same key from both object paths. */
      CMPIData key1 = CMGetKeyAt(object1, i, &keyname, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(keyname)) return 0;
      CMPIData key2 = CMGetKey(object2, CMGetCharPtr(keyname), &status);
      if (status.rc != CMPI_RC_OK) return 0;

      /* Check if both keys are not nullValue and have the same value. '^' = XOR. */
      if ((CMIsNullValue(key1) ^ CMIsNullValue(key2)) || !_CMSameValue(key1,key2)) return 0;
   }

   /* If got here then everything matched! */
   return 1;
}

/* ------------------------------------------------------------------------- */

/* Compare two CIM instances to see if they are identical. */
int _CMSameInstance( CMPIInstance * instance1, CMPIInstance * instance2 )
{
   CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations */
   int i;
  
   /* Check that the two instances have the same object path. */
   CMPIObjectPath * objectpath1 = CMGetObjectPath(instance1, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath1)) return 0;
   CMPIObjectPath * objectpath2 = CMGetObjectPath(instance2, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath2)) return 0;
   if (!_CMSameObject(objectpath1, objectpath2)) return 0;
 
   /* Check if the two instances have the same number of properties. */
   int numproperties1 = CMGetPropertyCount(instance1, &status);
   if (status.rc != CMPI_RC_OK) return 0;
   int numproperties2 = CMGetPropertyCount(instance2, &status);
   if (status.rc != CMPI_RC_OK) return 0;
   if (numproperties1 != numproperties2) return 0;

   /* Go through the properties for the 1st instance and compare to the 2nd instance. */
   for (i=0; i<numproperties1; i++) {
      CMPIString * propertyname = NULL;

      /* Retrieve the same property from both instances */
      CMPIData property1 = CMGetPropertyAt(instance1, i, &propertyname, &status);
      if ((status.rc != CMPI_RC_OK) || CMIsNullObject(propertyname)) return 0;
      CMPIData property2 = CMGetProperty(instance2, CMGetCharPtr(propertyname), &status);
      if (status.rc != CMPI_RC_OK) return 0;

      /* Check if both properties are not nullValue and have the same value. '^' = XOR */
      if ((CMIsNullValue(property1) ^ CMIsNullValue(property2)) || !_CMSameValue(property1,property2)) return 0;
   }

   /* If got here then everything matched! */
   return 1;
}

/* ------------------------------------------------------------------------- */

/* Get the string name of a CMPIStatus return code. */
const char * _CMPIrcName ( CMPIrc rc )
{
   switch (rc) {
      case CMPI_RC_OK:                               return "CMPI_RC_OK";
      case CMPI_RC_ERR_FAILED:                       return "CMPI_RC_ERR_FAILED";
      case CMPI_RC_ERR_ACCESS_DENIED:                return "CMPI_RC_ERR_ACCESS_DENIED";
      case CMPI_RC_ERR_INVALID_NAMESPACE:            return "CMPI_RC_ERR_INVALID_NAMESPACE";
      case CMPI_RC_ERR_INVALID_PARAMETER:            return "CMPI_RC_ERR_INVALID_PARAMETER";
      case CMPI_RC_ERR_INVALID_CLASS:                return "CMPI_RC_ERR_INVALID_CLASS";
      case CMPI_RC_ERR_NOT_FOUND:                    return "CMPI_RC_ERR_NOT_FOUND";
      case CMPI_RC_ERR_NOT_SUPPORTED:                return "CMPI_RC_ERR_NOT_SUPPORTED";
      case CMPI_RC_ERR_CLASS_HAS_CHILDREN:           return "CMPI_RC_ERR_CLASS_HAS_CHILDREN";
      case CMPI_RC_ERR_CLASS_HAS_INSTANCES:          return "CMPI_RC_ERR_CLASS_HAS_INSTANCES";
      case CMPI_RC_ERR_INVALID_SUPERCLASS:           return "CMPI_RC_ERR_INVALID_SUPERCLASS";
      case CMPI_RC_ERR_ALREADY_EXISTS:               return "CMPI_RC_ERR_ALREADY_EXISTS";
      case CMPI_RC_ERR_NO_SUCH_PROPERTY:             return "CMPI_RC_ERR_NO_SUCH_PROPERTY";
      case CMPI_RC_ERR_TYPE_MISMATCH:                return "CMPI_RC_ERR_TYPE_MISMATCH";
      case CMPI_RC_ERR_QUERY_LANGUAGE_NOT_SUPPORTED: return "CMPI_RC_ERR_QUERY_LANGUAGE_NOT_SUPPORTED";
      case CMPI_RC_ERR_INVALID_QUERY:                return "CMPI_RC_ERR_INVALID_QUERY";
      case CMPI_RC_ERR_METHOD_NOT_AVAILABLE:         return "CMPI_RC_ERR_METHOD_NOT_AVAILABLE";
      case CMPI_RC_ERR_METHOD_NOT_FOUND:             return "CMPI_RC_ERR_METHOD_NOT_FOUND";
//      case CMPI_RC_DO_NOT_UNLOAD:                    return "CMPI_RC_DO_NOT_UNLOAD";
//      case CMPI_RC_NEVER_UNLOAD:                     return "CMPI_RC_NEVER_UNLOAD";
//      case CMPI_RC_ERR_INVALID_HANDLE:               return "CMPI_RC_ERR_INVALID_HANDLE";
//      case CMPI_RC_ERR_INVALID_DATA_TYPE:            return "CMPI_RC_ERR_INVALID_DATA_TYPE";
      case CMPI_RC_ERROR_SYSTEM:                     return "CMPI_RC_ERROR_SYSTEM";
      case CMPI_RC_ERROR:                            return "CMPI_RC_ERROR";

      default: return "UNKNOWN";
   }
}

/* ------------------------------------------------------------------------- */

/* Get the string name of a CMPIType identifier. */
const char * _CMPITypeName ( CMPIType type )
{
   switch(type) {
      case CMPI_null:            return "CMPI_null";
      case CMPI_boolean:         return "CMPI_boolean";
      case CMPI_char16:          return "CMPI_char16";
      case CMPI_real32:          return "CMPI_real32";
      case CMPI_real64:          return "CMPI_real64";
      case CMPI_uint8:           return "CMPI_uint8";
      case CMPI_uint16:          return "CMPI_uint16";
      case CMPI_uint32:          return "CMPI_uint32";
      case CMPI_uint64:          return "CMPI_uint64";
      case CMPI_sint8:           return "CMPI_sint8";
      case CMPI_sint16:          return "CMPI_sint16";
      case CMPI_sint32:          return "CMPI_sint32";
      case CMPI_sint64:          return "CMPI_sint64";
      case CMPI_instance:        return "CMPI_instance";
      case CMPI_ref:             return "CMPI_ref";
      case CMPI_args:            return "CMPI_args";
      case CMPI_class:           return "CMPI_class";
      case CMPI_filter:          return "CMPI_filter";
      case CMPI_enumeration:     return "CMPI_enumeration";
      case CMPI_string:          return "CMPI_string";
      case CMPI_chars:           return "CMPI_chars";
      case CMPI_dateTime:        return "CMPI_dateTime";
      case CMPI_ptr:             return "CMPI_ptr";
      case CMPI_charsptr:        return "CMPI_charsptr";
      case CMPI_ARRAY:           return "CMPI_ARRAY";

      default: return "UNKNOWN"; 
   }
}

/* ------------------------------------------------------------------------- */

/* Get a string representation of a CMPIData value. */
/* Note - the caller *MUST* free this string when they done with it. */
char * _CMPIValueToString ( CMPIData data )
{
   char * valuestring = NULL;
   int len;
   
   /* First make sure there is a value. */
   if (CMIsNullValue(data)) return NULL;

   /* Format the value string as appropriate for the value type. */
   switch(data.type) {
      case CMPI_char16: {
         len = 2 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%c", data.value.char16);
         return valuestring;
      }
      case CMPI_sint8: {
         len = 5 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%d", data.value.sint8);
         return valuestring;
      }
      case CMPI_uint8: {
         len = 4 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%u", data.value.uint8);
         return valuestring;
      }
      case CMPI_sint16: {
         len = 7 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%d", data.value.sint16);
         return valuestring;
      }
      case CMPI_uint16: {
         len = 6 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%u", data.value.uint16);
         return valuestring;
      }
      case CMPI_sint32: {
         len = 12 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%d", data.value.sint32);
         return valuestring;
      }
      case CMPI_uint32: {
         len = 11 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%u", data.value.uint32);
         return valuestring;
      }
      case CMPI_sint64: {
         len = 21 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%" PRId64, data.value.sint64);
         return valuestring;
      }
      case CMPI_uint64: {
         len = 20 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%" PRIu64, data.value.uint64);
         return valuestring;
      }
      case CMPI_string: {
         if (CMIsNullObject(data.value.string)) return NULL;
         char * str = CMGetCharPtr(data.value.string);
         if (str == NULL) return NULL;
         valuestring = (char *)strdup(str);
         return valuestring;
      }
      case CMPI_boolean: {
         len = 6 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%s", (data.value.boolean)? "TRUE":"FALSE");
         return valuestring;
      }
      case CMPI_real32: {
         len = 20 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%.16e", data.value.real32);
         return valuestring;
      }
      case CMPI_real64: {
         len = 36 * sizeof(char);
         valuestring = (char *)malloc(len);
         if (valuestring == NULL) return NULL;
         snprintf(valuestring, len, "%.32e", data.value.real64);
         return valuestring;
      }
      case CMPI_dateTime: {
         CMPIStatus status = {CMPI_RC_OK, NULL};
         if (CMIsNullObject(data.value.dateTime)) return NULL;

         /* Get the string representation of CMPI_dateTime value. */
         CMPIString * datetimestr = CMGetStringFormat(data.value.dateTime, &status);
         if ((status.rc != CMPI_RC_OK) || CMIsNullObject(datetimestr)) return NULL;
         valuestring = (char *)strdup(CMGetCharPtr(datetimestr));
         return valuestring;
      }

      default:
         return NULL;
   }
}

/* Create Instance IDs of the following forms
 * Syntax of id parameter
 * Xen:<domain id>
 * Xen:<domain id>[/<device id>]
 */
int _CMPICreateNewSystemInstanceID(char *buf, int buf_len, char *systemid)
{
    snprintf(buf, buf_len, "Xen:%s", systemid);
    return 1;
}
int _CMPICreateNewDeviceInstanceID(char *buf, int buf_len, char *systemid, char *deviceid)
{
    snprintf(buf, buf_len, "Xen:%s/%s", systemid, deviceid);
    return 1;
}

/*
 * Return the 'system' component of InstanceID property values.
 * The system name will be returned in buffer buf of buf_len.
 * Syntax of id parameter
 * Xen:<domain name>[/<device name>]
 */
char * _CMPIStrncpySystemNameFromID(char *buf, const char *id, size_t buf_len)
{
   char *begin;
   char *end;
   char *tmp = strdup(id);
   
   if (tmp == NULL)
      return NULL;

   if ((begin = strchr(tmp, ':')) == NULL) {
      free(tmp);
      return NULL;
   }
   begin++;
   
   if ((end = strrchr(begin, '/')))
      *end = '\0';
      
   if (strlen(begin) >= buf_len) {
      free(tmp);
      return NULL;
   }

   memset(buf, 0, buf_len);
   strncpy(buf, begin, buf_len);
   free(tmp);
   return buf;
}


/*
 * Return the 'device name' component of InstanceID property values.
 * The device name be returned in buffer buf of buf_len.
 * Syntax of id parameter
 * Xen:<domain name>[\<device name>]
 */
char * _CMPIStrncpyDeviceNameFromID(char *buf, const char *id, size_t buf_len)
{
   char *dev_name;
   
   if (id == NULL)
      return NULL;

   if ((dev_name = strrchr(id, '/')) == NULL)
      return NULL;
   
   dev_name++;
   
   if (strlen(dev_name) >= buf_len)
      return NULL;

   memset(buf, 0, buf_len);
   strncpy(buf, dev_name, buf_len);
   return buf;
}
