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
// Authors:       Jim Fehlig, <jfehlig@novell.com>
// Contributors:
// Description:   Association provider for Xen_VSMSElementCapabilities.
// ============================================================================

#include <string.h>

/* Include the required CMPI data types, function headers, and macros */
#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"


// ----------------------------------------------------------------------------
// COMMON GLOBAL VARIABLES
// ----------------------------------------------------------------------------

/* Handle to the CIM broker. Initialized when the provider lib is loaded. */
static const CMPIBroker *_BROKER;

/* Include utility functions */
#include "cmpiutil.h"

/* Include _SBLIM_TRACE() logging support */
#include "cmpitrace.h"


// ============================================================================
// CMPI ASSOCIATION PROVIDER FUNCTION TABLE
// ============================================================================

// ----------------------------------------------------------------------------
// Info for the class supported by the association provider
// ----------------------------------------------------------------------------
                                                                                                                                 
/* Name of the left and right hand side classes of this association. */
static char * _ASSOCCLASS = "Xen_VSMSElementCapabilities";
static char * _LHSCLASSNAME = "Xen_VirtualSystemManagementService";
static char * _RHSCLASSNAME = "Xen_VirtualSystemManagementCapabilities";
static char * _LHSPROPERTYNAME = "ManagedElement"; 
static char * _RHSPROPERTYNAME = "Capabilities";
static char * _LHSNAMESPACE = "root/cimv2"; 
//static char * _RHSNAMESPACE = "root/cimv2"; 


// ----------------------------------------------------------------------------
// AssociationCleanup()
// Perform any necessary cleanup immediately before this provider is unloaded.
// ----------------------------------------------------------------------------
static CMPIStatus AssociationCleanup(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
        CMPIBoolean terminating)   /* [in] True if MB is terminating */
{
   CMPIStatus status = { CMPI_RC_OK, NULL };	/* Return status of CIM operations. */

   _SBLIM_ENTER("AssociationCleanup");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));

   /* Nothing needs to be done for cleanup. */
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// AssociatorNames()
// ----------------------------------------------------------------------------
static CMPIStatus AssociatorNames(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains source nameSpace, classname and object path. */
		const char * assocClass,
		const char * resultClass,
		const char * role,
		const char * resultRole)
{
   CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
   char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
   char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
   char *targetclass; 				/* Class of the target object(s). */


   _SBLIM_ENTER("AssociatorNames");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultClass=\"%s\"", resultClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultRole=\"%s\"", resultRole));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

   /* Check that the requested association class, if any, is supported. */
   if (assocClass != NULL) {
      CMPIObjectPath * assoc = CMNewObjectPath(_BROKER, nameSpace, _ASSOCCLASS, NULL);
      if (!CMClassPathIsA(_BROKER, assoc, assocClass, NULL)) {
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                      ("--- Unrecognized association class. Ignoring request."));
         goto exit;
      }
   }

   /* Check that the reference matches the required role, if any. */
   if ((role != NULL) && strcmp(role, sourceclass) != 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Reference does not match required role. Ignoring request."));
      goto exit;
   }

   /* Determine the target class from the source class. */
   if (strcmp(sourceclass, _LHSCLASSNAME) == 0) {
      targetclass = _RHSCLASSNAME;
   } else if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
      targetclass = _LHSCLASSNAME;
   } else {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unrecognized source class. Ignoring request."));
      goto exit;
   }
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\"", targetclass));

   /* Create an object path for the result class. */
   CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, nameSpace, targetclass, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
      goto exit;
   }

   /* Get the list of all target class object paths from the CIMOM. */
   CMPIEnumeration * objectpaths = CBEnumInstanceNames(_BROKER, context, objectpath, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpaths)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CBEnumInstanceNames() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
      goto exit;
   }

   /* Return all object paths that exactly match the target class and resultClass, if specified. */
   while (CMHasNext(objectpaths, NULL)) {
      CMPIData data = CMGetNext(objectpaths, NULL);
      char *class = CMGetCharPtr(CMGetClassName(data.value.ref, NULL));

      /* Ignore possible instances of source class. */
      if (strcmp(class,sourceclass) &&
          (resultClass == NULL || CMClassPathIsA(_BROKER, data.value.ref, resultClass, NULL))) {

         CMReturnObjectPath(results, data.value.ref);
      }
   }

   CMReturnDone(results);

exit:
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// Associators()
// ----------------------------------------------------------------------------
static CMPIStatus Associators(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the source nameSpace, classname and object path. */
		const char *assocClass,
		const char *resultClass,
		const char *role,
		const char *resultRole,
		const char ** properties)		/* [in] List of desired properties (NULL=all). */
{
   CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
   char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
   char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
   char *targetclass;                           /* Class of the target object(s). */

   _SBLIM_ENTER("Associators");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultClass=\"%s\"", resultClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- resultRole=\"%s\"", resultRole));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

   /* Check that the requested association class, if any, is supported. */
   if (assocClass != NULL) {
      CMPIObjectPath * assoc = CMNewObjectPath(_BROKER, nameSpace, _ASSOCCLASS, NULL);
      if (!CMClassPathIsA(_BROKER, assoc, assocClass, NULL)) {
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                      ("--- Unrecognized association class. Ignoring request."));
         goto exit;
      }
   }

   /* Check that the reference matches the required role, if any. */
   if ((role != NULL) && strcmp(role, sourceclass) != 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Reference does not match required role. Ignoring request."));
      goto exit;
   }

   /* Determine the target class from the source class. */
   if (strcmp(sourceclass, _LHSCLASSNAME) == 0) {
      targetclass = _RHSCLASSNAME;
   } else if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
      targetclass = _LHSCLASSNAME;
   } else {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unrecognized source class. Ignoring request."));
      goto exit;
   }
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\"", targetclass));

   /* Create an object path for the result class. */
   CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, nameSpace, targetclass, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
      goto exit;
   }

   /* Get the list of all target class instances from the CIMOM. */
   CMPIEnumeration * instances = CBEnumInstances(_BROKER, context, objectpath, NULL, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(instances)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CBEnumInstances() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
      goto exit;
   }

   /* Return all instances that exactly match the target class and resultClass, if specified. */
   while (CMHasNext(instances, NULL)) {
      CMPIData data = CMGetNext(instances, NULL);
      CMPIObjectPath *op = CMGetObjectPath(data.value.inst,NULL);
      char *class = CMGetCharPtr(CMGetClassName(CMGetObjectPath(data.value.inst,NULL), NULL));

      /* Ignore possible instances of source class. */
      if (strcmp(class,sourceclass) &&
          (resultClass == NULL || CMClassPathIsA(_BROKER, op, resultClass, NULL))) {
         CMReturnInstance(results, data.value.inst);
      }
   }

   CMReturnDone(results);

exit:
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// ReferenceNames()
// ----------------------------------------------------------------------------
static CMPIStatus ReferenceNames(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the source nameSpace, classname and object path. */
		const char *assocClass, 
		const char *role)
{
   CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
   char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
   char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
   char *targetclass;                           /* Class of the target object(s). */

   _SBLIM_ENTER("ReferenceNames");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

   /* Check that the requested association class, if any, is supported. */
   if (assocClass != NULL) {
      CMPIObjectPath * assoc = CMNewObjectPath(_BROKER, nameSpace, _ASSOCCLASS, NULL);
      if (!CMClassPathIsA(_BROKER, assoc, assocClass, NULL)) {
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                      ("--- Unrecognized association class. Ignoring request."));
         goto exit;
      }
   }

   /* Check that the reference matches the required role, if any. */
   if ((role != NULL) && strcmp(role, sourceclass) != 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Reference does not match required role. Ignoring request."));
      goto exit;
   }

   /* Determine the target class from the source class. */
   if (strcmp(sourceclass, _LHSCLASSNAME) == 0) {
      targetclass = _RHSCLASSNAME;
   } else if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
      targetclass = _LHSCLASSNAME;
   } else {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unrecognized source class. Ignoring request."));
      goto exit;
   }
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\"", targetclass));

   /* Create an object path for the result class. */
   CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, nameSpace, targetclass, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
      goto exit;
   }

   /* Get the list of all target class object paths from the CIMOM. */
   CMPIEnumeration * objectpaths = CBEnumInstanceNames(_BROKER, context, objectpath, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpaths)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CBEnumInstanceNames() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
      goto exit;
   }

   /* Return all object paths that exactly match the target class and resultClass, if specified. */
   while (CMHasNext(objectpaths, NULL)) {
      CMPIData data = CMGetNext(objectpaths, NULL);

      if((CMClassPathIsA(_BROKER, data.value.ref, targetclass, NULL)) &&
         (!CMClassPathIsA(_BROKER, data.value.ref, sourceclass, NULL))) {
         /* Create an object path for the association. Note that the association
          * objects should exist in 'virt namespace' not the host namespace.
          */
         CMPIObjectPath * refobjectpath = CMNewObjectPath(_BROKER, _LHSNAMESPACE, _ASSOCCLASS, &status);
         if ((status.rc != CMPI_RC_OK) || CMIsNullObject(refobjectpath)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
            goto exit;
         }

         /* Assign the references in the association appropriately. */
         if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
            CMAddKey(refobjectpath, _RHSPROPERTYNAME, &reference, CMPI_ref);
            CMAddKey(refobjectpath, _LHSPROPERTYNAME, &data.value.ref, CMPI_ref);
         } else {
            CMAddKey(refobjectpath, _RHSPROPERTYNAME, &data.value.ref, CMPI_ref);
            CMAddKey(refobjectpath, _LHSPROPERTYNAME, &reference, CMPI_ref);
         }

         CMReturnObjectPath(results, refobjectpath);
      }
   }

exit:
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// References()
// ----------------------------------------------------------------------------
static CMPIStatus References(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context,		/* [in] Additional context info, if any. */
		const CMPIResult * results,		/* [out] Results of this operation. */
		const CMPIObjectPath * reference,	/* [in] Contains the nameSpace, classname and desired object path. */
		const char *assocClass,
		const char *role,
		const char **properties)		/* [in] List of desired properties (NULL=all). */
{
   CMPIStatus status = { CMPI_RC_OK, NULL };    /* Return status of CIM operations. */
   char *nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
   char *sourceclass = CMGetCharPtr(CMGetClassName(reference, &status)); /* Class of the source reference object */
   char *targetclass;                           /* Class of the target object(s). */

   _SBLIM_ENTER("References");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- assocClass=\"%s\"", assocClass));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- role=\"%s\"", role));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- sourceclass=\"%s\"", sourceclass));

   /* Check that the requested association class, if any, is supported. */
   if (assocClass != NULL) {
      CMPIObjectPath * assoc = CMNewObjectPath(_BROKER, nameSpace, _ASSOCCLASS, NULL);
      if (!CMClassPathIsA(_BROKER, assoc, assocClass, NULL)) {
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                      ("--- Unrecognized association class. Ignoring request."));
         goto exit;
      }
   }

   /* Check that the reference matches the required role, if any. */
   if ((role != NULL) && strcmp(role, sourceclass) != 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Reference does not match required role. Ignoring request."));
      goto exit;
   }

   /* Determine the target class from the source class. */
   if (strcmp(sourceclass, _LHSCLASSNAME) == 0) {
      targetclass = _RHSCLASSNAME;
   } else if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
      targetclass = _LHSCLASSNAME;
   } else {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- Unrecognized source class. Ignoring request."));
      goto exit;
   }
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- targetclass=\"%s\"", targetclass));

   /* Create an object path for the result class. */
   CMPIObjectPath * objectpath = CMNewObjectPath(_BROKER, nameSpace, targetclass, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpath)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CMNewObjectPath() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIObjectPath");
      goto exit;
   }

   /* Get the list of all target class object paths from the CIMOM. */
   CMPIEnumeration * objectpaths = CBEnumInstanceNames(_BROKER, context, objectpath, &status);
   if ((status.rc != CMPI_RC_OK) || CMIsNullObject(objectpaths)) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                   ("--- CBEnumInstanceNames() failed - %s", CMGetCharPtr(status.msg)));
      CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot enumerate target class");
      goto exit;
   }

   /* Return all object paths that exactly match the target class and resultClass, if specified. */
   while (CMHasNext(objectpaths, NULL)) {
      CMPIData data = CMGetNext(objectpaths, NULL);

      if((CMClassPathIsA(_BROKER, data.value.ref, targetclass, NULL)) &&
         (!CMClassPathIsA(_BROKER, data.value.ref, sourceclass, NULL))) {
         /* Create an instance for the association. Note that the association
          * objects should exist in 'virt namespace' not the host namespace.
          */
         CMPIInstance * refinstance = _CMNewInstance(_BROKER, _LHSNAMESPACE, _ASSOCCLASS, &status);
         if ((status.rc != CMPI_RC_OK) || CMIsNullObject(refinstance)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                         ("--- CMNewInstance() failed - %s", CMGetCharPtr(status.msg)));
            CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERROR, "Cannot create new CMPIInstance");
            goto exit;
         }

         /* Assign the references in the association appropriately. */
         if (strcmp(sourceclass, _RHSCLASSNAME) == 0) {
            CMSetProperty(refinstance, _RHSPROPERTYNAME, &reference, CMPI_ref);
            CMSetProperty(refinstance, _LHSPROPERTYNAME, &data.value.ref, CMPI_ref);
         } else {
            CMSetProperty(refinstance, _RHSPROPERTYNAME, &data.value.ref, CMPI_ref);
            CMSetProperty(refinstance, _LHSPROPERTYNAME, &reference, CMPI_ref);
         }

         CMReturnInstance(results, refinstance);
      }
   }
exit:
   _SBLIM_RETURNSTATUS(status);
}


// ----------------------------------------------------------------------------
// AssociationInitialize()
// Perform any necessary initialization immediately after this provider is
// first loaded.
// ----------------------------------------------------------------------------
static void AssociationInitialize(
		CMPIAssociationMI * self,	/* [in] Handle to this provider (i.e. 'self'). */
		const CMPIContext * context)		/* [in] Additional context info, if any. */
{
   _SBLIM_ENTER("AssociationInitialize");
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
   //   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));

   /* Nothing needs to be done to initialize this provider */
   _SBLIM_RETURN();
}


// ============================================================================
// CMPI ASSOCIATION PROVIDER FUNCTION TABLE SETUP
// ============================================================================
CMAssociationMIStub( , Xen_VSMSElementCapabilities, _BROKER, AssociationInitialize(&mi, ctx));
