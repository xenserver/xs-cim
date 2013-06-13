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
#include <string.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>
#include <pthread.h>

/* Include the required CMPI data types, function headers, and macros */
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cmpilify.h"
#include "cmpitrace.h"
#include "Xen_Job.h"
#include "xen_utils.h"

/* Async methods */
static CMPI_THREAD_RETURN job_worker_thread_func(void *unused);

/* Globals */
pthread_mutex_t g_workitem_list_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_cond_mutex            = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_workitem_list_non_empty = PTHREAD_COND_INITIALIZER;

/* Asynchronous jobs are queued to a workitem list which is then
 *  handled by exactly one thread to prevent dom0 resource consumption
 *  issues (Dom0 being where this CIM provider is expected to run) 
 */
typedef struct _workitem
{
    void *jobdata;
    struct _workitem *next;
} workitem;

workitem* g_workitem_list = NULL;
int g_bAsync_worker_working_on_job = 0;
pthread_t g_async_worker_thread;

static Xen_job* job_alloc(
    xen_utils_session *session,
    char *job_name,
    char *domain_name,
    async_task  callback,
    void *job_context,
    char *cn,
    char *ns);

void job_free(Xen_job *job);

/*
* return int - non-zero on success, zero on error
*/
int _workitem_enqueue (
    Xen_job *job
    )
{
    pthread_mutex_lock(&g_workitem_list_mutex);
    workitem *item = calloc(1, sizeof(workitem));
    item->next = NULL;
    item->jobdata = job;

    if(g_workitem_list == NULL)
        g_workitem_list = item;
    else {
        /* FIFO list */
        workitem* lastitem = g_workitem_list;
        while(lastitem->next != NULL)
            lastitem = lastitem->next;
        lastitem->next = item;
    }
    pthread_mutex_unlock(&g_workitem_list_mutex);

    pthread_mutex_lock(&g_cond_mutex);
    pthread_cond_signal(&g_workitem_list_non_empty);
    pthread_mutex_unlock(&g_cond_mutex);
    return 1;
}

/*
* return int - 0 on success, non-zero on error
*/
int _workitem_dequeue (
    Xen_job **job
    )
{
    int rc = -1;
    workitem* item = NULL;

    pthread_mutex_lock(&g_cond_mutex);
    while(1) {
        pthread_mutex_lock(&g_workitem_list_mutex);
        if(g_workitem_list != NULL) {
            item = g_workitem_list;
            g_bAsync_worker_working_on_job = 1;
            g_workitem_list = item->next;
            *job = (Xen_job *)item->jobdata;
            free(item);
            rc = 0;
        }
        pthread_mutex_unlock(&g_workitem_list_mutex);
        if(*job == NULL) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Async worker Could not find a job to work on... BLOCKING"));
            /* Thread cancel could occur here */
            g_bAsync_worker_working_on_job = 0;
            pthread_cond_wait(&g_workitem_list_non_empty, 
                              &g_cond_mutex); /* cond_mutex is unlocked before blocking */
            /* once we return the cond_mutex is locked again */
        }
        else{
             // found something on the queue, time to return
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Async worker Found a job to work on EXECUTING JOB...... "));
            break;
        }
    }
    pthread_mutex_unlock(&g_cond_mutex);
    return rc;
}

/* This is kind of ugly, but there were issues with pthread stack corrption
   when the SIGCHLD signal was not being handled by the thread that forked 
   the child process */
void _signalhandler(int signum)
{
}

void _handle_signal()
{
    signal(SIGCHLD, _signalhandler);
}

int _get_tid()
{
    pthread_t tid = pthread_self();
    return tid;
}

/*
* return bool - true on success, false on error
* This is used to prevent a library from unloading when an async job has been scheduled/is running
*/
bool jobs_running()
{
    return ((g_workitem_list != NULL) || 
            (g_bAsync_worker_working_on_job));
}

/**
* @brief jobs_initialize - 
*   Perform any 1 time initialization called during provider
*   load from the main thread
* @param None
* @return int - zero on error, non-zero on success
*/
int jobs_initialize()
{   
    int rc = 1;
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Initializing on thread %d", _get_tid()));
    pthread_mutex_lock(&g_workitem_list_mutex);
    if(g_async_worker_thread == 0) {
        signal(SIGCHLD, SIG_IGN); /* ignore SIGCHLD on all threads except the one that does the fork */
        int err = pthread_create(&g_async_worker_thread, NULL, 
                                 job_worker_thread_func, NULL);
        if(err) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt async start error %d ", err));
            rc = 0;
        }
    }
    pthread_mutex_unlock(&g_workitem_list_mutex);
    return rc;
}

/**
* @brief jobs_uninitialize - Perform any 1 time cleanup, 
*   called during unload of provider
* @param None
* @return int - 0 on error, non-zero on success
*/
int jobs_uninitialize()
{
//#if 0
    // since this gets called every time on a provider unload, not a good idea to 
    // tear down the one thread and re-create it every time
    pthread_mutex_lock(&g_workitem_list_mutex);
    if(!jobs_running()) {
        pthread_cancel(g_async_worker_thread);
        pthread_join(g_async_worker_thread, NULL); // wait till the async worker finishes
    }
    pthread_mutex_unlock(&g_workitem_list_mutex);
//#endif
    return 1;
}

/**
* @brief job_worker_thread_func
*   This is the worker thread that pulls jobs off the queue and
*   executes their callback.
* @return None
*/
static CMPI_THREAD_RETURN job_worker_thread_func(void *unused)
{
    (void)unused;

    CMPIStatus status;
    Xen_job *job = NULL;
    _handle_signal();

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("async worker thread id %d", _get_tid()));

    while(1) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("async worker thread woke up"));
        if(_workitem_dequeue(&job) == 0)
        {
            /* prepare CMPI on this thread to be able to handle the async call */
            CBAttachThread(job->broker, job->call_context);
            async_task callback_func = job->callback;

            /* Call one callback function and wait till it finishes, 
             * so we dont take up too many resources. */
            struct xen_call_context *call_ctx = NULL;
            if (xen_utils_get_call_context(job->call_context, &call_ctx, &status)) {
                xen_utils_session *session = NULL;
                /* validate the user and get a fresh xen session */
                if (xen_utils_validate_session(&session, call_ctx)) {
                    job->session = session;
                    callback_func(job);
                    xen_utils_cleanup_session(session);
                } else {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("ERROR: couldnt get xen session"));
                }
                xen_utils_free_call_context(call_ctx);
            } else {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("ERROR: couldnt get call context : %d", status.rc));
            }

            CBDetachThread(job->broker, job->call_context);
            job_free(job);
        }
        g_bAsync_worker_working_on_job = 0;
        job = NULL;
    }

    return NULL;
}

/*============================================================================
 * Job CIM class Support
 *===========================================================================*/
static Xen_job* job_alloc(
    xen_utils_session *session,
    char *job_name,
    char *domain_name,
    async_task  callback,
    void *job_context,
    char *cn,
    char *ns)
{
    Xen_job *job = malloc(sizeof(Xen_job));
    if(job == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("ERROR: No memory available to allocate job"));
        return NULL;
    }
    job->job_name       = strdup(job_name);
    job->domain_name    = strdup(domain_name);
    job->callback       = callback; 
    job->job_context    = job_context; /* job owner frees this */
    job->ref_cn         = strdup(job_name);
    job->ref_ns         = strdup(ns);

    /* We create a xen task for book-keeping purposes, this will always be in 
       'pending' state and Xen will garbage collect it after 24 hours */
    if(!xen_task_create(session->xen, &job->task_handle, job_name, domain_name)) {
        job_free(job);
        job = NULL;
        goto Exit;
    }

    /* update the xen task record withi minimal information */
    xen_string_string_map *other_config = NULL;
    xen_utils_add_to_string_string_map("PercentComplete", "0", &other_config);
    xen_utils_add_to_string_string_map("CIMJobState", "2", &other_config); //JobState_New = 2
    xen_task_set_other_config(session->xen, job->task_handle, other_config);
    xen_string_string_map_free(other_config);

    char *job_uuid = NULL;
    if(xen_task_get_uuid(session->xen, &job_uuid, job->task_handle) && job_uuid) {
        strncpy(job->uuid, job_uuid, UUID_LEN);
        job->uuid[UUID_LEN] = '\0';
        free(job_uuid);
    }

Exit:
    if(!session->xen->ok) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        if(job) {
            session->xen->ok = true;
            if(job->task_handle)
                xen_task_destroy(session->xen, job->task_handle);
            free(job);
            job = NULL;
            session->xen->ok = false;
        }
    }
    return job;
}

void job_free(Xen_job* job)
{
    if(job == NULL) 
        return;

    if(job->job_name)
        free(job->job_name);
    if(job->domain_name)
        free(job->domain_name);
    if(job->ref_cn)
        free(job->ref_cn);
    if(job->ref_ns)
        free(job->ref_ns);
    if(job->task_handle)
        xen_task_free(job->task_handle);

    free(job);
}
/*@brief job_change_state - update the state of a job CIM object
* @param job - pointer to the job whose state is being updated
* @param state - state to be updated with
* @param percen_complete - self explanatory
* @param error_code - errors in the task, if any
* @param description - error or general description for task progress 
* @return none
*/
void job_change_state(
    Xen_job* job, 
    xen_utils_session *session,
    JobState state, 
    int percent_complete, 
    int error_code,
    char* description)
{
    xen_task_record *task_rec = NULL;

    /* reset any prior errors */
    RESET_XEN_ERROR(session->xen);
    if(!xen_task_get_record(session->xen, &task_rec, job->task_handle))
        goto Exit;

    /* update the xen task record - that's where we persist the task information 
       The only RW property on the xen_task object is the other-config field.
       So, use that to persist all information we care about */
    char buf[100];
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%d", percent_complete);
    xen_utils_add_to_string_string_map("PercentComplete", buf, &task_rec->other_config);
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%d", state);
    xen_utils_add_to_string_string_map("CIMJobState", buf, &task_rec->other_config);
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%d", error_code);
    xen_utils_add_to_string_string_map("ErrorCode", buf, &task_rec->other_config);
    if(description) {
        if(error_code != 0) {
            /* this is an error - set the error description and clear the description */
            xen_utils_add_to_string_string_map("Description", "", &task_rec->other_config);
            xen_utils_add_to_string_string_map("ErrorDescription", description, &task_rec->other_config);
        }
        else {
            /* this is just a job update - clear the error description and set the description */
            xen_utils_add_to_string_string_map("Description", description, &task_rec->other_config);
            xen_utils_add_to_string_string_map("ErrorDescription", "", &task_rec->other_config);
        }
    }
    xen_task_set_other_config(session->xen, job->task_handle, task_rec->other_config);
Exit:
    if(!session->xen->ok)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    if(task_rec)
        xen_task_record_free(task_rec);
}
/*@brief job_create - create a new job, queue it to be executed on a separate thread
*                     and return the job object path
* @param broker - Broker for CMPI services
* @param context - caller's context (username etc)
* @param job_name - name for the job's CIM class
* @param domain_name - Name (UUID) of the VM the job is for
* @param callback - providier callback function to be called on the separate thread
* @param job_context - providier callback context to be passed back to the function
* @param op - CIM object path for the newly created job object, to be tracked by the client
* @return int - 1 for success, 0 for failure
*/
int job_create(
    const CMPIBroker    *broker,
    const CMPIContext   *context,
    xen_utils_session   *session,
    char                *job_name,
    char                *domain_name,
    async_task          callback,
    void                *job_context,
    CMPIObjectPath      **job_instance_op, /* out */
    CMPIStatus          *status)           /* out */
{
    Xen_job *job = job_alloc(session, job_name, domain_name, callback, job_context, job_name, XEN_CLASS_NAMESPACE);
    if(!job) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not alloc job"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR, "Cannot alloc new Job");
        return 0;
    }

    /* prepare the job to be attached to a different thread */
    job->broker = broker;
    job->call_context = CBPrepareAttachThread(job->broker, context);

    *job_instance_op = CMNewObjectPath(job->broker, DEFAULT_NS, job_name, status);
    char buf[MAX_INSTANCEID_LEN+1];
    _CMPICreateNewSystemInstanceID(buf, sizeof(buf)/sizeof(buf[0]), job->uuid);
    CMAddKey(*job_instance_op, "InstanceID", (CMPIValue *)buf, CMPI_chars);

    /* enqueue the job to be handled on a separate thread */
    _workitem_enqueue(job);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("NewJob reference=\"%s\"", CMGetCharPtr(CDToString(job->broker, *job_instance_op, NULL))));
    return 1;
}

