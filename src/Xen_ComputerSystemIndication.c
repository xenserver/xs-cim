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
// Authors:       Dr. Gareth S. Bestor, <bestor@us.ibm.com>
// Contributors:
// Description:
// ============================================================================

#include <string.h>
#include <unistd.h>

/* Include the required CMPI data types, function headers, and macros */
#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"
#include "xen_utils.h"

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
// CMPI INDICATION PROVIDER FUNCTION TABLE
// ============================================================================

/* Flag indicating if indications are currently enabled */
static int enabled = 0;
static int pollingInterval = 10;

/* Number of active indication filters (i.e. # registered subscriptions) */
static int numActiveFilters = 0;

/* Handle to the asynchronous indication generator thread */
static CMPI_THREAD_TYPE indicationThreadId = 0;

static char * _NAMESPACE = "root/cimv2";
//static char * _CLASSNAME = "Xen_ComputerSystem";

// ----------------------------------------------------------------------------
// _indicationThread()
// Runtime thread to periodically poll to generate indications.
// ----------------------------------------------------------------------------
CMPI_THREAD_RETURN _indicationThread( void * parameters )
{
    CMPIContext * cmpi_context = (CMPIContext *)parameters; /* Indication thread context */
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations */
    CMPIInstance * indication;          /* CIM instance for each new indication */
    struct xen_string_set *classes = NULL;
    xen_utils_session *session = NULL;

    _SBLIM_ENTER("_indicationThread");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- original context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, cmpi_context, NULL))));

    /* Register this thread to the CMPI runtime. */
    CBAttachThread(_BROKER, cmpi_context);

    struct xen_call_context *ctx = NULL;
    if(!xen_utils_get_call_context(cmpi_context, &ctx, &status))
    {
        goto exit;
    }

    /* Initialized Xen session object. */
    if(!session)
        xen_utils_xen_init2(&session, ctx);

    if(!xen_utils_validate_session(&session, ctx))
    {
        goto exit;
    }

    /* Register with xen all the events we are interested in */
    classes = xen_string_set_alloc(0);
    //classes->contents[0] = "VM";
    if(!xen_event_register(session->xen, classes))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Xen Event registration failed ......"));
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        goto exit;
    }


    /* Periodically poll while there is at least one indication subscriber. */
    while(numActiveFilters > 0)
    {

        /* Check if indications are still enabled. */
        if(!enabled)
        {
            sleep(pollingInterval);
            continue;
        }

        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Waiting for xen events......"));
        struct xen_event_record_set *events = NULL;
        if(!xen_event_next(session->xen, &events))
        { /* Will block until events are available */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("xen_event_next returned error:"));
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            goto exit;
        }

        if(events)
        {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Xen returned %d events of interest......", events->size));
            int i=0;
            for(i=0; i<events->size; i++)
            {

                CMPIInstance *affected_vm = _CMNewInstance(_BROKER, _NAMESPACE, "Xen_ComputerSystem", &status);
                CMSetProperty(affected_vm, "Name", events->contents[i]->obj_uuid, CMPI_chars);

                if(events->contents[i]->operation == XEN_EVENT_OPERATION_ADD)
                {
                    /* InstCreation: If no matching old instance then this is a brand new instance. */
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- New instance!"));
                    indication = _CMNewInstance(_BROKER, _NAMESPACE, "Xen_ComputerSystemCreation", &status);
                    if(status.rc != CMPI_RC_OK) goto exit;

                    /* Set the indication properties. */
                    CMSetProperty(indication, "SourceInstance",(CMPIValue *)affected_vm, CMPI_instance);
                }
                else if(events->contents[i]->operation == XEN_EVENT_OPERATION_MOD)
                {
                    /* InstModification: Check if old instance has different property values. */
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- Modified instance!"));
                    indication = _CMNewInstance(_BROKER, _NAMESPACE, "Xen_ComputerSystemModification", &status);
                    if(status.rc != CMPI_RC_OK) goto exit;

                    /* Set the indication properties. */
                    CMSetProperty(indication, "SourceInstance",(CMPIValue *)affected_vm, CMPI_instance);
                    //CMSetProperty(indication, "PreviousInstance",(CMPIValue *)&(oldinstancedata.value.inst), CMPI_instance);
                }
                else if(events->contents[i]->operation == XEN_EVENT_OPERATION_DEL)
                {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- Deleted instance!"));
                    indication = _CMNewInstance(_BROKER, _NAMESPACE, "Xen_ComputerSystemDeletion", &status);
                    if(status.rc != CMPI_RC_OK) goto exit;

                    /* Set the indication properties. */
                    CMSetProperty(indication, "SourceInstance",(CMPIValue *)affected_vm, CMPI_instance);
                }
                else
                {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- Not sure what event operation %d this is!", events->contents[i]->operation));
                    continue; // continue to the next event in the loop
                }

                /* Deliver the indication to all subscribers. */
                /* THIS CALL WILL HANG IF DNS CANNOT RESOLVE THE CLIENT'S SYSTEMNAME OR 
                   IF THE SFCB INDICATION PROVIDER IS IN THE SAME PROCESS GROUP AS XEN-CIM */
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- Delivering indication"));
                status = CBDeliverIndication(_BROKER, cmpi_context, _NAMESPACE, indication);
                if(status.rc != CMPI_RC_OK)
                {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Failed to deliver indication"));
                    goto exit;
                }

            }
            xen_event_record_set_free(events);
        }
        else
        {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_WARNING, ("Xen returned 0 events of interest......"));
        }
    }

    exit:
    /* unregister all the events from xen */
    if(classes)
    {
        if(session)
            xen_event_unregister(session->xen, classes);
        xen_string_set_free(classes);
    }

    /* Un-Register this thread from the CMPI runtime. */
    CBDetachThread(_BROKER, cmpi_context);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- No more active filters, killing the indication thread"));

    /* close this xen session */
    if(session)
    {
        xen_utils_xen_close2(session);
        session = NULL;
    }

    if(ctx) xen_utils_free_call_context(ctx);
    indicationThreadId = 0;
    _SBLIM_RETURN(NULL);
}

// ----------------------------------------------------------------------------
// IndicationCleanup()
// Perform any necessary cleanup immediately before this provider is unloaded.
// ----------------------------------------------------------------------------
static CMPIStatus IndicationCleanup(
    CMPIIndicationMI * self,          /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context,      /* [in] Additional context info, if any. */
    CMPIBoolean terminating)
{
    CMPIStatus status = { CMPI_RC_OK, NULL};    /* Return status of CIM operations. */

    _SBLIM_ENTER("IndicationCleanup");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));

    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// AuthorizeFilter()
// Check whether the requested filter is valid/permitted.
// ----------------------------------------------------------------------------
static CMPIStatus AuthorizeFilter(
    CMPIIndicationMI * self,    /* [in] Handle to this provider (i.e. 'self') */
    const CMPIContext * context,      /* [in] Additional context info, if any */
    const CMPISelectExp * filter,     /* [in] Indication filter query */
    const char * eventtype,     /* [in] Target indication class(es) of filter. */
    const CMPIObjectPath * reference, /* [in] Namespace and classname of monitored class */
    const char * owner )        /* [in] Name of principle requesting the filter */
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of authorization */
    CMPIBoolean authorized = CMPI_true;
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char * classname = CMGetCharPtr(CMGetClassName(reference, NULL)); /* Target class. */

    _SBLIM_ENTER("AuthorizeFilter");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- filter=\"%s\"", CMGetCharPtr(CMGetSelExpString(filter, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- eventtype=\"%s\"", eventtype));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- destination owner=\"%s\"", owner));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- classname=\"%s\"", classname));

    /* Check that the filter indication class is supported. */
    if((strcmp(eventtype,"Xen_ComputerSystemCreation") != 0) &&
        (strcmp(eventtype,"Xen_ComputerSystemDeletion") != 0) &&
        (strcmp(eventtype,"Xen_ComputerSystemModification") != 0))
    {
        authorized = CMPI_false;
        status.rc = CMPI_RC_ERR_ACCESS_DENIED;
    }

    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// MustPoll()
// Specify if the CIMOM should generate indications instead, by polling the
// instance data for any changes.
// ----------------------------------------------------------------------------
static CMPIStatus MustPoll(
    CMPIIndicationMI * self,        /* [in] Handle to this provider (i.e. 'self') */
    const CMPIContext * context,          /* [in] Additional context info, if any */
    const CMPISelectExp * filter,         /* [in] Indication filter query */
    const char * eventtype,         /* [in] Filter target class(es) */
    const CMPIObjectPath * reference )     /* [in] Namespace and classname of monitored class */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char * classname = CMGetCharPtr(CMGetClassName(reference, NULL)); /* Target class. */

    _SBLIM_ENTER("MustPoll");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- filter=\"%s\"", CMGetCharPtr(CMGetSelExpString(filter, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- eventtype=\"%s\"", eventtype));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- classname=\"%s\"", classname));

    /* Polling not required for this indication provider */
    status.rc = CMPI_RC_ERR_NOT_SUPPORTED;
    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// ActivateFilter()
// Add another subscriber and start generating indications.
// ----------------------------------------------------------------------------
static CMPIStatus ActivateFilter(
    CMPIIndicationMI * self,        /* [in] Handle to this provider (i.e. 'self') */
    const CMPIContext * context,          /* [in] Additional context info, if any */
    const CMPISelectExp * filter,         /* [in] Indication filter query */
    const char * eventtype,         /* [in] Filter target class(es) */
    const CMPIObjectPath * reference,     /* [in] Namespace and classname of monitored class */
    const CMPIBoolean first )             /* [in] Is this the first filter for this eventtype? */
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char * classname = CMGetCharPtr(CMGetClassName(reference, NULL)); /* Target class. */

    _SBLIM_ENTER("ActivateFilter");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- filter=\"%s\"", CMGetCharPtr(CMGetSelExpString(filter, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- eventtype=\"%s\"", eventtype));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- first=%s", (first)? "TRUE":"FALSE"));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- classname=\"%s\"", classname));

    numActiveFilters++;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- numActiveFilters=%d", numActiveFilters));

    /* Startup the indication generator if it isn't already running */
    if(indicationThreadId == 0)
    {
        /* Get the context for the new indication generator thread */
        CMPIContext * indicationContext = CBPrepareAttachThread(_BROKER, context);
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- indicationcontext=\"%s\"", CMGetCharPtr(CDToString(_BROKER, indicationContext, NULL))));

        /* Statup a new non-detached thread to run the indication generator */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Starting up indication generator thread"));
        indicationThreadId = _BROKER->xft->newThread(_indicationThread, indicationContext, 0);
    }

    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// DeActivateFilter()
// Remove a subscriber and if necessary stop generating indications.
// ----------------------------------------------------------------------------
static CMPIStatus DeActivateFilter(
    CMPIIndicationMI * self,        /* [in] Handle to this provider (i.e. 'self') */
    const CMPIContext * context,          /* [in] Additional context info, if any */
    const CMPISelectExp * filter,         /* [in] Indication filter query */
    const char * eventtype,         /* [in] Filter target class(es) */
    const CMPIObjectPath * reference,     /* [in] Namespace and classname of monitored class */
    CMPIBoolean last )              /* [in] Is this the last filter for this eventtype? */
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char * classname = CMGetCharPtr(CMGetClassName(reference, NULL)); /* Target class. */

    _SBLIM_ENTER("DeActivateFilter");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- context=\"%s\"", CMGetCharPtr(CDToString(_BROKER, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- filter=\"%s\"", CMGetCharPtr(CMGetSelExpString(filter, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- eventtype=\"%s\"", eventtype));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(_BROKER, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- last=%s", (last)? "TRUE":"FALSE"));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- namespace=\"%s\"", nameSpace));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("--- classname=\"%s\"", classname));

    if(numActiveFilters == 0)
    {
        //      deactivated = CMPI_false;
        CMSetStatusWithChars(_BROKER, &status, CMPI_RC_ERR_FAILED, "No active filters");
        goto exit;
    }

    numActiveFilters--;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG,("--- numActiveFilters=%d", numActiveFilters));

    /* If no active filters then shutdown the indication generator thread */
    if((numActiveFilters == 0) && indicationThreadId != 0)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- Shutting down indication generator thread"));
        _BROKER->xft->cancelThread(indicationThreadId);
        indicationThreadId = 0;
    }
    exit:
    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// EnableIndications()
// ----------------------------------------------------------------------------
static CMPIStatus EnableIndications(
    CMPIIndicationMI * self,
    const CMPIContext *context )
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations */
    _SBLIM_ENTER("EnableIndications");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));

    /* Enable indication generation */
    enabled = 1;
    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// DisableIndications()
// ----------------------------------------------------------------------------
static CMPIStatus DisableIndications(
    CMPIIndicationMI * self,
    const CMPIContext *context )
{
    CMPIStatus status = {CMPI_RC_OK, NULL}; /* Return status of CIM operations */
    _SBLIM_ENTER("DisableIndications");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));

    /* Disable indication generation */
    enabled = 0;

    _SBLIM_RETURNSTATUS(status);
}

// ----------------------------------------------------------------------------
// IndicationInitialize()
// Perform any necessary initialization immediately after this provider is
// first loaded.
// ----------------------------------------------------------------------------
static void IndicationInitialize(
    const CMPIIndicationMI * self,          /* [in] Handle to this provider (i.e. 'self'). */
    const CMPIContext * context)          /* [in] Additional context info, if any. */
{
    _SBLIM_ENTER("IndicationInitialize");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%d\"", context));

    _SBLIM_RETURN();
}

CMIndicationMIStub( , Xen_ComputerSystemIndication, _BROKER, IndicationInitialize(&mi, ctx));
