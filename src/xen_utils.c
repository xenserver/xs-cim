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
// Contributors:  Raj Subrahmanian <raj.subrahmanian@unisys.com>
// Description:   Utilitiy functions built on top of libxen for use in all
//                providers.
// ============================================================================

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>

/* Init the parser for libxenapi */
#include <libxml/parser.h>
#include <curl/curl.h>

#include <cmpidt.h>
#include <cmpiutil.h>
#include <cmpimacs.h>
#include "xen_utils.h"
#include "provider_common.h"
//#include "cmpilify.h"

/* Include _SBLIM_TRACE() logging support */
#include "cmpitrace.h"

#include "Xen_KVP.h"

// XXX I don't like having these declarations here.
extern void Xen_SettingDatayyrestart(FILE *);
extern int Xen_SettingDatayyparseinstance(const CMPIBroker *, CMPIInstance **);

/* Global variables for reference counting this library's use. */
static pthread_mutex_t ref_count_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int ref_count = 0;

///////////////////////////////////////////////////////////////////////////
/* Private functions */
char XmlToAscii(const char **XmlStr);
char * XmlToAsciiStr(const char *XmlStr);

/*
 * Provider's initialization of xen. To be called in the provider's 
 * initialization routine.
 *
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_xen_init()
{
    pthread_mutex_lock(&ref_count_lock);
    if (ref_count == 0) {
        // 
        // This is a workaround for a bug in libxml2 where it calls pthread_key_create
        // but never calls pthread_key_delete in xmlCleanupParser, which causes 
        // the pthreads library to call the destructor on thread exit after libxml2
        // is unloaded.
        // 
        // InitParser is now called in the main cimserver thread
        // This assumes the providers share the process with the main process.
        // 
        //xmlInitParser();
        xen_init();
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ref_count++;
    pthread_mutex_unlock(&ref_count_lock);

    return 1;
}
int xen_utils_xen_init2(
    xen_utils_session **session, 
    struct xen_call_context *id)
{
    xen_utils_xen_init();
    return xen_utils_get_session(session, id->user, id->pw);
}

/*
 * One time uninitialization of xen.
 * Providers should invoke this function when their Cleanup() method is 
 * invoked by the cimom.
 */
void xen_utils_xen_close()
{
    pthread_mutex_lock(&ref_count_lock);
    ref_count--;
    if (ref_count == 0) {
        xen_fini();
        // See note on xmlInitParser above
        //xmlCleanupParser();
        curl_global_cleanup();
    }
    pthread_mutex_unlock(&ref_count_lock);
}

void xen_utils_xen_close2(
    xen_utils_session *session
    )
{
    xen_utils_cleanup_session(session);
    xen_utils_xen_close();
    pthread_mutex_unlock(&ref_count_lock);
}

/* 
 * Note: The CIMOM has been tweaked to pass in the password along with the 
 * username in the form "user pass" in the CMPIPrincipal context entry.
 * Space is an invalid character for usernames, hence the use as a delimiter. 
 * We need both the username and password to pass it along to xen-api so
 * authentication/authorization can be delegated to xapi 
 */
int xen_utils_get_call_context(
    const CMPIContext *cmpi_ctx, 
    struct xen_call_context **ctx,
    CMPIStatus* status
    )
{
    int rc = 0;
    CMPIData principal = cmpi_ctx->ft->getEntry(cmpi_ctx, "CMPIPrincipal", status);
    if (status->rc == CMPI_RC_OK) {
        char *str = strdup(CMGetCharPtr(principal.value.string));
        *ctx = calloc(1, sizeof(struct xen_call_context));
        (*ctx)->user = str;
        str = strchr(str, ' ');
        if(str == NULL) {
            /* couldnt find password */
            CMSetStatus(status, CMPI_RC_ERR_ACCESS_DENIED);
        }
        else {
            *str++ = '\0';
            (*ctx)->pw = str;
            rc = 1;
        }
    }
    if(rc != 1) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Couldnt get Caller Principal: ERROR %d", status->rc));
    }
    return rc;
}

void xen_utils_free_call_context(
    struct xen_call_context *ctx
    )
{
    if(ctx) {
        /* user and pw are both pointing to the same buffer */
        free(ctx->user);
        ctx->user = NULL;
        free(ctx);
    }
}

int xen_utils_get_host_address(xen_utils_session *session, xen_vm vm_ref, char**address) {
  int rc = 0;
  xen_vm_record *vm_rec;
  
  if (!xen_vm_get_record(session->xen, &vm_rec, vm_ref)){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not find record for VM %s", vm_ref));
    goto exit;
  }

  xen_host_record_opt *host = vm_rec->resident_on;
  
  if (!host->u.handle) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Error: could not find a host reference"));
    goto exit;
  }

  if (!xen_host_get_address(session->xen, &(*address), host->u.handle)){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not find a host address for %s", host->u.handle));
    goto exit;
  }

  rc = 1;
  
 exit:
  if (vm_rec)
    xen_vm_record_free(vm_rec);

  return rc;
}

int xen_utils_get_hostname(xen_utils_session *session, xen_vm vm_ref, char **hostname){
  xen_vm_record *vm_rec;

  if (!xen_vm_get_record(session->xen, &vm_rec, vm_ref)){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not find record for VM %s", vm_ref));
    goto exit;
  }

  xen_host_record_opt *host = vm_rec->resident_on;
  
  if (!host->u.handle) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not find host reference"));
    goto exit;
  }


  if (!xen_host_get_hostname(session->xen, &(*hostname), host->u.handle)){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not find hostname for host %s", host->u.handle));
    goto exit;
  }


  xen_vm_record_free(vm_rec);

  return 1;

exit:
      if(vm_rec)
	xen_vm_record_free(vm_rec);
      
      return 0;
}


/* 
 Helper HTTP client to talk to xapi
*/
typedef struct
{
    xen_result_func func;
    void *handle;
} xen_comms;

/*
Helper HTTP client to talk to KVP daemon
*/
typedef struct
{
  char *memory;
  size_t size;
}kvp_comms;

typedef struct
{
  const char *readptr;
  long sizeleft;
}kvp_post_comms;

static size_t
write_to_buffer(void *data, size_t size, size_t nmemb, void *stream){

  size_t len = size * nmemb;

  kvp_comms *mem = (kvp_comms *) stream;

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Size: %d", mem->size));
  mem->memory = realloc(mem->memory, mem->size + len + 1 + 1);

  if(mem->memory == NULL) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error, couldn't allocate memory"));
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), data, len);

  mem->size += len;
  mem->memory[mem->size] = 0;
  
  return len;
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
  kvp_post_comms *data = (kvp_post_comms *)userp;

  if(size * nmemb < 1)
    return 0;

  if (data->sizeleft) {
    *(char *)ptr = data->readptr[0];
      data->readptr++;
      data->sizeleft--;
      return 1;
  }

  return 0;

}


long post_to_url(const char *url, char *data){
  CURL *curl;
  CURLcode res = 0;
  long http_code = 0;

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Post to URL %s", url));

  kvp_post_comms *data_obj = (kvp_post_comms *)malloc(sizeof(kvp_post_comms));

  data_obj->readptr = data;
  data_obj->sizeleft = strlen(data);

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Value to Post: %s", data_obj->readptr));

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("post_to_url"));
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, data_obj);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_obj->sizeleft);

    res = curl_easy_perform(curl);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Curl RC: %d", res));
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("HTTP RC: %d", http_code));

    curl_easy_cleanup(curl);
  } else {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Curl could not be initialized"));
  }

  if (data_obj)
    free(data_obj);

  return http_code;
}


long get_from_url(char *url, char** buffer){
  CURL *curl;
  CURLcode res = 0;
  long http_code = 0;

  kvp_comms *chunk = malloc(sizeof(kvp_comms));
  chunk->memory = NULL;
  chunk->size = 0;

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("get_from_url"));

  curl = curl_easy_init();

  if (curl && chunk) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);  
    curl_easy_cleanup(curl);

    *buffer = chunk->memory;
    
    /* Free Chunk - but leave caller to free buffer */
    free(chunk);
  } else {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not init curl or chunk"));
  }
  
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Curl Return Code: %d", res));
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("HTTP Return Code: %d", http_code));
  
  return http_code;

 }

int initialise_kvp(kvp **kvp_obj) {

  *kvp_obj = (kvp *)malloc(sizeof(kvp));
  
  if(*kvp_obj){
    (*kvp_obj)->key = NULL;
    (*kvp_obj)->value = NULL;
    (*kvp_obj)->vm_uuid = NULL;
    return 1;
  } else {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not allocate memory"));
    return 0;
  }
  
}

int initialise_kvp_set(kvp_set** set){
  *set = malloc(sizeof(kvp_set));

  if(*set){
    (*set)->size = 0;
    (*set)->contents = NULL;
    return 1;
  } else {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not allocate memory"));
    return 0;
  }

}

int xen_utils_free_kvp(kvp *kvp)
{

  if (kvp) {
    if (kvp->key){
        free(kvp->key);
        kvp->key=NULL;
    }
    if (kvp->value){
        free(kvp->value);
        kvp->value=NULL;
    }
    if (kvp->vm_uuid){
        free(kvp->vm_uuid);
        kvp->vm_uuid=NULL;
    }
    free(kvp);
  }
  return 0;
}

int xen_utils_create_kvp(char *key, char *value, char *vm_uuid, kvp **kvp_obj) {

    /* Initialise the KVP*/
    if(initialise_kvp(kvp_obj)) {
        (*kvp_obj)->key = strdup(key);
        (*kvp_obj)->vm_uuid = strdup(vm_uuid);
        (*kvp_obj)->value = strdup(value);

        return 1;
    } else {
        return 0;
    }
}

int xen_utils_kvp_copy(kvp *orig, kvp **new) {

    /* Note: the allocated memory should
       be free'd by the caller */
    if(initialise_kvp(new)) {
        (*new)->key = strdup(orig->key);
        (*new)->vm_uuid = strdup(orig->vm_uuid);
        (*new)->value = strdup(orig->value);

        return 1;
    } else {
        return 0;
    }
}


int xen_utils_free_kvpset(kvp_set *set){
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_utils_free_kvpset (%d)", set->size));
  int i;
  /*Iterate through set*/
  for (i = 0; i < set->size; i++){
    /*Free the actual KVP*/
    kvp *kvp = &set->contents[i];
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Free %s %s %s",
					   kvp->key,
					   kvp->value,
					   kvp->vm_uuid));
    
    /* Re-implement xen_utils_free_kvp since for a set we cannot
       call free on the kvp structure itself. We must instead free
       the structure contents, and finally free the whole set contents */

    if (kvp) {
      if (kvp->key)
	free(kvp->key);
      if (kvp->value)
	free(kvp->value);
      if(kvp->vm_uuid)
	free(kvp->vm_uuid);
    }
       
  }

  /*Free the memory allocated for KVP pointers */
  free(set->contents);

  /*Free rest of the structure*/
  free(set);
  return 0;
}

int add_to_kvp_set(kvp_set* set, kvp* pair){

   /* New set size will be one more object in length */
   int new_set_size = sizeof(kvp) * (set->size + 1);

  /*Increase the size of the array*/
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("realloc %d", new_set_size));
  kvp *_tmp = realloc(set->contents, new_set_size);

  if (!_tmp){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Insufficient memory!"));
  } 

  set->contents = (kvp *)_tmp;


  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("size %d", set->size));
  /*Increment size to reflect one extra item */
  set->size++;

  /* Don't produce a copy of KVP due to the fact
     we have already realloc'ed memory for the set */

  kvp *kvp_item = &set->contents[set->size-1];

  kvp_item->key = strdup(pair->key);
  kvp_item->value = strdup(pair->value);
  kvp_item->vm_uuid = strdup(pair->vm_uuid);

  return 0;
}

int xen_utils_append_kvp_set(kvp_set *dest, kvp_set *src) {
    
   if (src->size == 0)
       return 0;
    int i;
   for (i=0; i < src->size; i++) {

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Appending item %d", i));
    kvp *tmp_kvp = &src->contents[i];
    add_to_kvp_set(dest, tmp_kvp);
   }

    return 0;
}

int xen_utils_kvp_compose_url(xen_utils_session *session,
			    char *vm_uuid,
			    char *key,
			    char **url,
			    char *cmd)
{
  xen_vm vm_ref = NULL;
  char *address = NULL;
  char *lowercasecmd = NULL;
  char *plugin = "services/plugin/xscim";
  char *xenref =  (char *)((session->xen)->session_id);
  int rc = 0;


  /*In case of failure, we nee to know whether
    we have already malloc'd any memory for URL*/

  *url = NULL;
 
  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Get by VM uuid"));
  if(!xen_vm_get_by_uuid(session->xen, &vm_ref, (char *)vm_uuid)){
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
		   ("--- xen_vm_get_by_uuid %s failed: \"%s\"", vm_uuid,
		    session->xen->error_description[0]));
      goto exit;
    }

    
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Get host address"));
	  if(!xen_utils_get_host_address(session, vm_ref, &address)){
	    /*Couldn't get host address*/
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: couldn't get host address"));
	    goto exit;
	  }


	  if ((strcmp(cmd, "GET") == 0) || (strcmp(cmd, "PUT") == 0) ) {
	  
	      if (key != NULL) {
		  *url = (char *)malloc(sizeof(char) * (30 + strlen(plugin) + strlen(address) + strlen(vm_uuid) + strlen(key) + strlen(xenref)));

		  if(*url) {
		      sprintf(*url, "http://%s/%s/vm/%s/key/%s?session_id=%s", address, plugin, (char *)vm_uuid, key, xenref);
		  } else {
		      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not compose URL"));
		      goto exit;
		  }
	      } else {
		  //No key has been supplied, form the 'GET ALL' URL

		*url = (char *)malloc(sizeof(char) * (25 + strlen(plugin) + strlen(address) + strlen(vm_uuid) + strlen(xenref)));

		  if(*url) {
		      sprintf(*url, "http://%s/%s/vm/%s?session_id=%s", address, plugin, (char *)vm_uuid, xenref);
		  } else {
		      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not compose URL"));
		      goto exit;
		  }
		  
	      }

	  } else if (strcmp(cmd, "SETUP") == 0 || strcmp(cmd, "PREPAREMIGRATION") == 0 || strcmp(cmd, "FINISHMIGRATION") == 0) {
          if (strcmp(cmd, "SETUP") == 0) {
              lowercasecmd="setup";
          }
          else if (strcmp(cmd, "PREPAREMIGRATION") == 0) {
              lowercasecmd="preparemigration";
          }  
          else if (strcmp(cmd, "FINISHMIGRATION") == 0) {
              lowercasecmd="finishmigration";
          } else {
              _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not create lower case cmd"));
              goto exit;
          }
          
	      *url = (char *)malloc(sizeof(char) * (30 + strlen(plugin) + strlen(address) + strlen(vm_uuid) + strlen(lowercasecmd) + strlen(xenref)));

	      if (*url) {
		  sprintf(*url, "http://%s/%s/vm/%s/cmd/%s?session_id=%s", address, plugin, (char *)vm_uuid, lowercasecmd, xenref);
	      } else {
		  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not compose URL"));
		  goto exit;
	      }

	  } else if (strcmp(cmd, "DEL") == 0) {

	    *url = (char *)malloc(sizeof(char) * (41 + strlen(plugin) + strlen(address) + strlen(vm_uuid) + strlen(key) + strlen(xenref)));

	      if (*url) {
		sprintf(*url, "http://%s/%s/vm/%s/key/%s/cmd/delete?session_id=%s", address, plugin, (char *)vm_uuid, key, xenref);
	      } else {
		  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not compose URL"));
		  goto exit;
	      }

	  }

	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Composed URL: %s", *url));
	  /* Operation Success */
	  rc = 1;

	exit:
	  if (address)
	    free(address);
	  if (vm_ref)
	    xen_vm_free(vm_ref);

	  if (!rc) {		
	    /* In the case of failure, we may have already malloc'ed the URL */
	    if (*url) {
	     free(*url);
         *url=NULL;
        }
	  }

  return rc;

}

Xen_KVP_RC xen_utils_delete_kvp(xen_utils_session *session,
			 kvp *kvp_obj)
{
  char *url = NULL;
  Xen_KVP_RC rc = Xen_KVP_RC_FAILED;

  if(!xen_utils_kvp_compose_url(session, kvp_obj->vm_uuid, kvp_obj->key, &url, "DEL"))
    goto exit;

   long http_code = 0;

   http_code = post_to_url(url, "");

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("HTTP RC: %d", http_code));

  if (http_code == 200)
    rc = Xen_KVP_RC_OK;

 exit:
  if (rc != Xen_KVP_RC_OK)
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("An error occurred"));
  if (url)
    free(url);

  return rc;
}
			    
Xen_KVP_RC xen_utils_push_kvp(xen_utils_session *session,
		       kvp *kvp_obj)
{
  char *url = NULL;
  Xen_KVP_RC rc;
  
  rc = Xen_KVP_RC_ERROR;

  if (kvp_obj->value == NULL) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Cannot push a KVP without a value"));
    goto exit;
  }

  if(!xen_utils_kvp_compose_url(session, kvp_obj->vm_uuid, kvp_obj->key, &url, "PUT"))
    goto exit;

  int http_rc = post_to_url(url, kvp_obj->value); 

  if (http_rc == 200)
    rc = Xen_KVP_RC_OK;

  if (http_rc == 401)
    rc = Xen_KVP_RC_FAILED;

 exit:
  /*free's*/
  if(url)
    free(url);

  return rc;

}


Xen_KVP_RC xen_utils_get_from_kvp_store(xen_utils_session *session, char *vm_uuid, char *key, char **value){

  char *buf;
  char *url = NULL;
  int rc = Xen_KVP_RC_ERROR;

  if(!xen_utils_kvp_compose_url(session, vm_uuid, key, &url, "GET"))
    goto exit;

  if (get_from_url(url, &buf) != 200){
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error: could not retrieve key from server (%s)", url));
    rc = Xen_KVP_RC_FAILED;
    goto exit;
  }
  
  *value = buf;
  rc = Xen_KVP_RC_OK;
  
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("exit"));
 exit:
  if (url)
    free(url);

  return rc;

}

Xen_KVP_RC xen_utils_get_kvp_store(char* url, char *vm_uuid, kvp_set **kvps) {

  char* buf = NULL;

  char *delimiter = "\n";
  char *sep = " ";

  Xen_KVP_RC rc = Xen_KVP_RC_ERROR;
  
  /* Retrive store over HTTP */
  if (get_from_url(url, &buf) != 200) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failed to get KVP store"));
    goto exit;
  }

  /* Setup the kvp set, Parse the store, returning a list of pairs */

  if (initialise_kvp_set(kvps) && buf && *buf != '\0') {
    char *tmp = NULL, *tok = NULL;
    char *key = NULL, *value = NULL;

    /* start with the series of key value\nkey value\nkey value...*/
    for (tok = strtok_r(buf, delimiter, &tmp);
	 tok;
	 tok = strtok_r(NULL, delimiter, &tmp)){
         /* we should have a 'key value' string here */ 
      char *tmp2 = NULL;

      kvp *pair;
      if(initialise_kvp(&pair)) {
          key = strtok_r(tok, sep, &tmp2);
          value = strtok_r(NULL, sep, &tmp2);

          pair->key = strdup(key);
          pair->value = strdup(value);
          pair->vm_uuid = strdup(vm_uuid);

          add_to_kvp_set(*kvps, pair);
          xen_utils_free_kvp(pair);

          kvp *tmpp = &(*kvps)->contents[(*kvps)->size-1];
          _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Key = %s, Val = %s", tmpp->key, tmpp->value));
      }
    }

  }

  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("There are %d items in the set", (*kvps)->size));

  rc = Xen_KVP_RC_OK;

 exit:
  if (buf)
    free(buf);

  return rc;

}

Xen_KVP_RC xen_utils_cmd_kvp_channel(xen_utils_session *session, char *vm_uuid, char *cmd){

    char *url;
    Xen_KVP_RC rc = Xen_KVP_RC_ERROR;

    if(!xen_utils_kvp_compose_url(session, vm_uuid, NULL, &url, cmd)) {
	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Could not compose url for setup"));
	goto exit;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Post to URL %s", url));
    //Post empty amount of data to URL
    if(post_to_url(url, "") != 200){
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Error: could not post to URL"));
      goto exit;
    }
    rc = Xen_KVP_RC_OK;

  exit:
    if (url)
	free(url);
    
    return rc;

}

Xen_KVP_RC xen_utils_setup_kvp_channel(xen_utils_session *session, char *vm_uuid){
    return xen_utils_cmd_kvp_channel(session, vm_uuid, "SETUP");
}

Xen_KVP_RC xen_utils_preparemigration_kvp_channel(xen_utils_session *session, char *vm_uuid){
    return xen_utils_cmd_kvp_channel(session, vm_uuid, "PREPAREMIGRATION");
}

Xen_KVP_RC xen_utils_finishmigration_kvp_channel(xen_utils_session *session, char *vm_uuid){
    return xen_utils_cmd_kvp_channel(session, vm_uuid, "FINISHMIGRATION");
}

static size_t
write_func(void *ptr, size_t size, size_t nmemb, xen_comms *comms)
{
    size_t n = size * nmemb;
    return comms->func(ptr, n, comms->handle) ? n : 0;
}

static CURL*
_initialize_curlsession(xen_utils_session* session)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
#ifdef CURLOPT_MUTE
    curl_easy_setopt(curl, CURLOPT_MUTE, 1);
#endif
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_func);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_URL, session->host_url);
    session->curl_handle = curl;

    return curl;
}

static void
_uninitialize_curlsession(xen_utils_session* session)
{
    if(session->curl_handle) {
        curl_easy_cleanup(session->curl_handle);
        session->curl_handle = NULL;
    }
}

static int
call_func(const void *data, size_t len, void *user_handle,
          void *result_handle, xen_result_func result_func)
{
    xen_comms comms = {
        .func = result_func,
        .handle = result_handle
    };

    char *useragent = "xs-cim\0";
    xen_utils_session *s = (xen_utils_session *)user_handle;
    curl_easy_setopt(s->curl_handle, CURLOPT_WRITEDATA, &comms);
    curl_easy_setopt(s->curl_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(s->curl_handle, CURLOPT_POSTFIELDSIZE, len);
    curl_easy_setopt(s->curl_handle, CURLOPT_USERAGENT, useragent);
    CURLcode result = curl_easy_perform(s->curl_handle);

    return result;
}

/*
 * Create a xen session with a remote host, using the xapi TCP port
 *
 * Returns 1 on success, 0 on failure.
 */
int xen_utils_get_remote_session(
    xen_utils_session **session, 
    char *remote_host_ip_addr,
    char *remote_login_user,
    char *remote_login_pw)
{
    xen_utils_session *s;
    *session = NULL;
    s = calloc(1, sizeof(xen_utils_session));
    if (s == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("No memory for Xen Daemon session object"));
        return 0;
    }

    snprintf(s->host_url, MAX_HOST_URL_LEN, "http://%s", remote_host_ip_addr);
    _initialize_curlsession(s);
    s->xen = xen_session_login_with_password(call_func, (void *)s, 
                 remote_login_user, remote_login_pw
#if XENAPI_VERSION > 400
                 ,xen_api_version_1_3
#endif
                 );
    if (s->xen == NULL || !s->xen->ok) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("Login to Xen Daemon %s failed using user %s (Error %s)", 
                     remote_host_ip_addr, remote_login_user, s->xen->error_description[0]));
        goto Error;
    }

    if (!xen_session_get_this_host(s->xen, &(s->host), s->xen)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failed to get session host to %s", remote_host_ip_addr));
        goto Error;
    }

    *session = s;
    return 1;

    Error:
    free(s);
    return 0;
}

/*
 * Create a xen session with the host
 *
 * Returns 1 on success, 0 on failure.
 */
int xen_utils_get_session(
    xen_utils_session **session, 
    char *user, 
    char *pw)
{
    xen_utils_session *s;

    *session = NULL;
    s = calloc(1, sizeof(xen_utils_session));
    if (s == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("No memory for Xen Daemon session object"));
        return 0;
    }
    /* direct the request at the local xapi */
    strncpy(s->host_url, "http://127.0.0.1", MAX_HOST_URL_LEN );
    _initialize_curlsession(s);
    s->xen = xen_session_login_with_password(call_func,
                 (void *)s,
                 user, pw
#if XENAPI_VERSION > 400
                 ,xen_api_version_1_3
#endif
                 );
    if (s->xen == NULL || !s->xen->ok) {
        if(s->xen->error_description_count >= 2) {
            if(strcmp(s->xen->error_description[0], "HOST_IS_SLAVE") == 0) {
                /* This host is not the master, re-route all future requests to the master */
                /* The master's ip address is in the error description */
                /* We could return the error as is and expect the caller to route to the master,
                   however, the WinRM client cannot handle HTTP redirects. Lets do it ourselves */
                char *master = strdup(s->xen->error_description[1]);
                xen_utils_cleanup_session(s);
                /* get a new session with the right master */
                if(!xen_utils_get_remote_session(&s, master, user, pw))
                    goto Error;
            }
            else
               goto Error;
        }
        else
            goto Error;
    } else {
        if (!xen_session_get_this_host(s->xen, &(s->host), s->xen)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Failed to get session host"));
            goto Error;
        }
    }
    *session = s;
    return 1;
Error:
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                 ("Login to Xen Daemon failed using user %s (Error %s)", 
                 user, s->xen->error_description[0]));
    xen_utils_cleanup_session(s);
    return 0;
}

/*
 * Validate xend session.  If sesssion is null, create one.
 * Session is ready for use on success.
 *
 * Returns a non-zero on success, 0 on failure.
 */
int xen_utils_validate_session(
    xen_utils_session **session, 
    struct xen_call_context *id)
{
    xen_utils_session *s;

    if (session == NULL)
        return 0;

    if (*session == NULL)
        if (!xen_utils_get_session(session, id->user, id->pw))
            return 0;

    s = *session;

    /* Clear any errors and attempt a simple call */
    s->xen->ok = 1;
    xen_host_free(s->host);
    s->host = NULL;
    if (xen_session_get_this_host(s->xen, &(s->host), s->xen) && s->xen->ok)
        return 1;

    /* Simple call failed.  Reconnect. */
    xen_session_logout(s->xen);
    free(s);
    *session = NULL;
    return xen_utils_get_session(session, id->user, id->pw);
}

/*
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_cleanup_session(
    xen_utils_session *session)
{
    if (session) {
        if(session->xen) {
            xen_session_logout(session->xen);
            session->xen = NULL;
        }
        if(session->host) {
            xen_host_free(session->host);
            session->host = NULL;
        }
        _uninitialize_curlsession(session);
        free(session);

    }
    return 1;
}

/*
 * Frees the memory used up by the xen session, 
 * without calling logout
*/
int xen_utils_free_session(
    xen_utils_session *session)
{
    if(session) {
        if(session->xen) {
            /* this comes straight from the SDK C binding */
            if (session->xen->error_description != NULL) {
                int i;
                for (i = 0; i < session->xen->error_description_count; i++)
                    free(session->xen->error_description[i]);
                free(session->xen->error_description);
            }
            free((char *)session->xen->session_id);
            free(session->xen);
            session->xen = NULL;
        }
        if(session->host) {
            xen_host_free(session->host);
            session->host = NULL;
        }
        _uninitialize_curlsession(session);
        free(session);
    }
    return 1;
}

/*
 * Generate exported functions for concatenating lists of references.
 */
XEN_UTILS_REF_LIST_CONCAT(xen_vm)
XEN_UTILS_REF_LIST_CONCAT(xen_vdi)
XEN_UTILS_REF_LIST_CONCAT(xen_vbd)
XEN_UTILS_REF_LIST_CONCAT(xen_vif)

/*
 * Generate exported functions for adding a reference (e.g. xen_vm) to a list
 * of references.
 */
XEN_UTILS_REF_LIST_ADD(xen_vm)
XEN_UTILS_REF_LIST_ADD(xen_vdi)
XEN_UTILS_REF_LIST_ADD(xen_vbd)
XEN_UTILS_REF_LIST_ADD(xen_vif)

/*
 * Generate exported functions for adding a device (e.g. vbd) to a list
 * of such devices.
 */
XEN_UTILS_DEV_LIST_ADD(xen_vdi_record)
XEN_UTILS_DEV_LIST_ADD(xen_vbd_record)
XEN_UTILS_DEV_LIST_ADD(xen_vif_record)

/****************************************************************
 *
 * Some usedul VM enumerate functions
 *
 ****************************************************************/
xen_vm_set* xen_utils_enum_domains(
    xen_utils_session *session,
    enum domain_choice choice) /* not used */
{
    xen_vm_set *resident_vms_including_templates;
    if (!xen_vm_get_all(session->xen, &resident_vms_including_templates)) {
        /* Error description in session object. */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                     ("--- xen_vm_get_all failed: \"%s\"", session->xen->error_description[0]));
        return NULL;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("got %d VMs from xen call", 
                                           resident_vms_including_templates->size));

    //xen_utils_log_domains(resident_vms_including_templates);
    return resident_vms_including_templates;
}

/* 
 * Logging the references contained within a VM set.
 * This allows us to trace progress through an enumeration.
 */

int xen_utils_log_domains(xen_vm_set *vm_set) {
  int i;
  for(i = 0; i < vm_set->size; i++) {
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VM Ref: %s", vm_set->contents[i]));
  }
  return 0;
}

/*
 * Retrieve the domain resources (a list of VMs) using the provided
 * session.
 * 
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_get_domain_resources(
    xen_utils_session *session,
    xen_domain_resources **resources,
    enum domain_choice templates_or_vms)
{
    if (session == NULL)
        return 0;

    /* malloc a new handle for the resources list. */
    *resources = (xen_domain_resources *)calloc(1, sizeof(xen_domain_resources));
    if (*resources == NULL)
        return 0;

    /* Get the list of Xen domains. */
    RESET_XEN_ERROR(session->xen);
    (*resources)->domains = xen_utils_enum_domains(session, templates_or_vms);
    if ((*resources)->domains == NULL)
        return 0;

    (*resources)->numdomains = (*resources)->domains->size;
    (*resources)->choice = templates_or_vms;

    return 1;
}

/* 
 * Get the time in millesconds since the
 * start of the day. This is a helper
 * function for tracing how long particular function calls
 * take. Due to it not taking into account days, tests
 * should not be performed across the day boundary.
 */
int xen_utils_get_time() {
  struct timeval tv;
  struct timezone tz;
  struct tm *tm;
  gettimeofday(&tv,&tz);
  tm=localtime(&tv.tv_sec);

  return (tm->tm_hour * 60 * 60 * 1000) + (tm->tm_min * 60 * 1000) +
    (tm->tm_sec * 1000) + (tv.tv_usec / 1000);
}

/*
 * Free the list of domain resources.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_free_domain_resources(
    xen_domain_resources *resources)
{
    if (resources) {
        if (resources->domains) {
            xen_vm_set_free(resources->domains);
            resources->domains = NULL;
        }

        free(resources);
        resources = NULL;
    }

    return 1;
}

/*
 * Retrieve the next domain from the list of domain resources.
 * Returns:
 * 1 on sucess
 * 0 when no more objects 
 * -1 on failure
 */
int xen_utils_get_next_domain_resource(
    xen_utils_session *session,
    xen_domain_resources *resources,
    xen_vm *resource_handle,
    xen_vm_record **resource_rec
    )
{
    if (session == NULL || resources == NULL)
        return 0;

    /* Check if reached the end of the list of Xen domain names. */
    if (resources->currentdomain == resources->numdomains){
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Reached the end of the list of Xen Domain names"));
      return 0;
    }

        while(true) {
            RESET_XEN_ERROR(session->xen);

	    *resource_handle = resources->domains->contents[resources->currentdomain];
	    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Current resource handle = %s", *resource_handle));
	    
	    if (!xen_vm_get_record(session->xen, resource_rec, *resource_handle)) {
	      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
			   ("--- xen_vm_get_record(next) failed: \"%s\"", 
			    session->xen->error_description[0]));

	      /* Because we are first enumerating a list of references, and then
	       * secondly asking for a record back for that reference, we have no
	       * guarentee that the object is still there. In the case of a failure,
	       * We should continue to enumerate the remaining objects*/
	      resources->currentdomain++; /*Move onto next domain*/
	      return -1; /*Returning a failure code */
	    }

	    if (resources->choice == all)
	      break;

            bool is_a_template = false, is_a_snapshot = false;
	    is_a_template = (**resource_rec).is_a_template;
	    is_a_snapshot = (**resource_rec).is_a_snapshot;

            if(resources->choice == templates_only && is_a_template) {
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Found a template"));
                break;
	    }
            else if(resources->choice == snapshots_only && is_a_snapshot) {
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Found a snapshot"));
                break;
	    }
            else if(resources->choice == vms_only && !is_a_snapshot && !is_a_template) {
		_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Found a VM"));
                break;
	    }
    
            /* didnt match up, continue to the next one and check if we are at the end */
	    if (resource_rec != NULL)
	      xen_vm_record_free(*resource_rec);
            if(++resources->currentdomain == resources->numdomains) {
	      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Didn't match choice with any of the options. Continue to the next object and check if we are at the end."));
                return 0;
	    }
        }

    /* Move the iterator to the next domain */
    resources->currentdomain++;

    return 1;
}

/*
 * Free the domain resource specified by resource.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_free_domain_resource(
    xen_vm resource_handle,
    xen_vm_record *resource_rec
    )
{
    /* resource_handle is freed when the list gets freed */
    if (resource_rec != NULL) {
        xen_vm_record_free(resource_rec);
    }

    return 1;
}

int xen_utils_get_domain_from_uuid(
    xen_utils_session *session,
    const char *uuid,
    xen_vm_record **domain_rec,
    xen_vm *vm)
{
    RESET_XEN_ERROR(session->xen);
    /* Get the domain data for the target domain name. */
    if (!xen_vm_get_by_uuid(session->xen, vm, (char *)uuid)) {
        /* Error is in session object! */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- xen_vm_get %s failed: \"%s\"", uuid, 
             session->xen->error_description[0]));
        return 0;
    }

    if (!xen_vm_get_record(session->xen, domain_rec, *vm)) {
        /* Error description in session object! */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- xen_vm_get_record failed: \"%s\"", 
             session->xen->error_description[0]));
        xen_vm_free(*vm);
        *vm = NULL;
        return 0;
    }
    return 1;
}

int xen_utils_get_domain_from_instanceID(
    xen_utils_session *session,
    char *instanceID, 
    xen_vm_record**domain,
    xen_vm* vm )
{
    char uuid[MAX_SYSTEM_NAME_LEN];
    RESET_XEN_ERROR(session->xen);

    /* Retrieve domain name from InstanceID. */
    if (!_CMPIStrncpySystemNameFromID(uuid, instanceID, MAX_SYSTEM_NAME_LEN)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- CMPIStrncpySystemNameFromId failed for instanceID %s", instanceID));
        return 0;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Found VM UUID %s", uuid));

    /* Get the domain data for the target domain name. */
    return xen_utils_get_domain_from_uuid(session, uuid, domain, vm);
}

/*
 * Retrieve a domain resource given a CIM_SettingData object path.
 * Domain record will be placed in out param "domain".
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_get_domain_from_sd_OP(
    xen_utils_session *session,
    xen_vm_record **domain_rec,
    xen_vm *domain_handle,
    const CMPIObjectPath *domOP)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};  /* Return status of CIM operations. */

    if (domain_rec == NULL) return 0;
    if (CMIsNullObject(domOP)) return 0;
    *domain_rec = NULL;

    /* Obtain the target domain name from the CMPIObjectPath "InstanceID" key. */
    CMPIData namedata = CMGetKey(domOP,"Name", &status);
    if ((status.rc != CMPI_RC_OK) || 
        CMIsNullValue(namedata) || 
        (namedata.type != CMPI_string)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- CMGetKey-Name failed"));
        return 0;
    }
    const char *uuid = CMGetCharPtr(namedata.value.string);
    RESET_XEN_ERROR(session->xen);
    return xen_utils_get_domain_from_uuid(session, uuid, domain_rec, domain_handle);
}

/*
 * Get Host for VM
*/
xen_host_record* xen_utils_get_domain_host(
    xen_utils_session *session,
    xen_vm_record *domain_rec
    )
{
    xen_host_record_opt* host_opt = domain_rec->resident_on;
    xen_host_record *host_rec = NULL;
    if (host_opt) {
        if (!host_opt->is_record)
            xen_host_get_record(session->xen, &host_rec, host_opt->u.handle);
        else
            host_rec = host_opt->u.record;

    }
    if (!host_rec) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, 
            ("Could not get host record for VM %s", domain_rec->uuid));
    }
    return host_rec;
}

/*
* Free the host record from above
*/
void xen_utils_free_domain_host(
    xen_vm_record *domain_rec,
    xen_host_record *host_rec
    )
{
    if (!domain_rec->resident_on->is_record)
        xen_host_record_free(host_rec);
}

/*
 * Determine if the domain specified by name active.
 * If active, isActive will be set to 1, otherwise 0.
 * Returns non-zero on success, 0 on failure.
 */
int xen_utils_is_domain_active(
    xen_utils_session *session,
    const char *uuid, 
    int *isActive)
{
    xen_vm vm;
    RESET_XEN_ERROR(session->xen);
    xen_vm_record *vm_rec = NULL; 

    if (!xen_utils_get_domain_from_uuid(session, uuid, &vm_rec, &vm))
        return 0;
    xen_vm_free(vm);
    if (vm_rec->power_state == XEN_VM_POWER_STATE_HALTED)
        *isActive = 0;
    else
        *isActive = 1;

    xen_vm_record_free(vm_rec);
    return 1;
}

/**********************************************************************
  Some useful string set and string map functions
***********************************************************************/
/*
 * Get value of key from map.
 * Returns pointer to value on success, NULL if key does not exist in map.
 */
char *xen_utils_get_from_string_string_map(
    xen_string_string_map *map,
    const char *key)
{
    int i = 0;
    if (!map || (map->size == 0))
        return NULL;

    while (i < map->size) {
        if (strcmp(key, map->contents[i].key) == 0)
            return map->contents[i].val;
        i++;
    }

    return NULL;
}

/* Clear out all entries in the string string map and set *map to NULL
 * Return 1 if successful, 0 if not
 */
int xen_utils_clear_string_string_map(
    xen_string_string_map **map)
{
    if (!map) {      // invalid param
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid Paramters, exiting"));
        return 0;
    }
    if (*map) {      // map is not deallocated yet
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Clearing map entries"));
        xen_string_string_map_free(*map);
        *map = NULL;
    }
    return 1;
}

/*
 * Add key/val to map.  If map contains key, update value for that key.
 * Returns non-zero on success, 0 on failure.  On failure, contens of map
 * is unchanged.
 */
int xen_utils_add_to_string_string_map(
    const char *key, 
    const char *val,
    xen_string_string_map **map)
{
    int i;

    if (*map == NULL) {
        *map = xen_string_string_map_alloc(1);
        if (*map == NULL) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                ("Could not add to string string map, out of memory"));
            return 0;
        }
        (*map)->size = 1;
        (*map)->contents[0].key = strdup(key);
        (*map)->contents[0].val = strdup(val);
        return 1;
    }

    /* Map is not empty.  Does key already exist? */
    for (i = 0; i < (*map)->size; i++) {
        if (strcmp((*map)->contents[i].key, key) == 0) {
            free((*map)->contents[i].val);
            (*map)->contents[i].val = strdup(val);
            return 1;
        }
    }

    /*  Grow the map and add the new entry */
    int new_len = sizeof(xen_string_string_map) +
                  (((*map)->size + 1) * sizeof(xen_string_string_map_contents));
    *map = realloc(*map, new_len);
    if (*map == NULL)
        return 0;

    (*map)->contents[(*map)->size].key = strdup(key);
    (*map)->contents[(*map)->size].val = strdup(val);
    (*map)->size++;

    return 1;
}
/*
 * Remove key/val from map. If map contains key, remove the key.
 * Returns non-zero on success, 0 on failure.  On failure, contents
 * of map is unchanged.
 */
int xen_utils_remove_from_string_string_map(
    char *key_to_remove,
    xen_string_string_map **map
    )
{
    char *map_str = xen_utils_flatten_string_string_map(*map);
    if (map_str) {
        int buflen = strlen(map_str);
        char *new_map_str = calloc(1, buflen);
        /* remove the parent_uuid item from the map */
        char *key_begin = strstr(map_str, key_to_remove);
        char *key_end = NULL;
        if(key_begin) {
            /* skip over the key item in the map */
            if(key_begin == map_str) 
                *key_begin++ = '\0';
            else {
                *(key_begin-1) = '\0'; /* get rid of the previous ',' as well */
                key_begin++;
            }
            key_end = strchr(key_begin, ',');
            if(key_end) {
                key_end++; /* skip over the trailing ',' */
                /* create new map string minus the key_to_remove mapping */
                if (strlen(map_str) > 0)
                    snprintf(new_map_str, (buflen-1), "%s,%s", map_str, key_end);
                else
                    snprintf(new_map_str, (buflen-1), "%s", key_end);
            }
            else
                snprintf(new_map_str, (buflen-1), "%s", map_str);
            /* recreate the new map minus the 'key_to_remove=value' string */
            xen_string_string_map_free(*map);
            *map = xen_utils_convert_string_to_string_map(new_map_str, ","); 
        }
        free(map_str);
        free(new_map_str);
    }
    else
        return 0;
    return 1;
}
/*
* Create a string map from a 'flattened' string map
* Converts a string of form key0=value0,key1=value1,...keyN=valueN
* into a xen_string_string_map
* caller will free the map returned
*/
xen_string_string_map* xen_utils_convert_string_to_string_map(
    const char *str,
    const char *delimiter
    )
{
    xen_string_string_map *map = xen_string_string_map_alloc(0);
    map->size = 0;
    if (str && *str != '\0') {
        char *tmp=NULL, *tok = NULL;
        char *tmp_str = strdup(str);
        /* start with the series of key0=value0,key1=value1,... string*/
        for (tok = strtok_r(tmp_str, delimiter, &tmp);
            tok;
            tok = strtok_r(NULL, delimiter, &tmp)) {
            /* we should have a 'key=value' substring here */
            char *tmp2 =NULL;
            char *key=strtok_r(tok, "=", &tmp2);
            char *val=strtok_r(NULL, "=", &tmp2);
            if (key && val) {
                if(*val == '\"') {
                    val++; /* skip over start/end string quotes (WBEM URI) */
                    val[strlen(val)-1] = '\0';
                }
                xen_utils_add_to_string_string_map(key, val, &map);
            }
        }
        free(tmp_str);
    }
    return map;
}

/*
 * Flatten a Xen API string-string map.  The flattened map will be in form
 * key0=value0,key1=value1,...,keyN=valueN
 * Returns char array containing flattened map on success, NULL on failure.
 * Caller is responsible for freeing memory.
 */
char *xen_utils_flatten_string_string_map(xen_string_string_map *map)
{
    unsigned int i = 0;
    unsigned int size = 0;
    char *flat_map;

    if (map == NULL || map->size == 0)
        return NULL;

    /* Calculate size of buffer needed to hold flattened map. */
    do {
        size += strlen(map->contents[i].key);
        size += strlen(map->contents[i].val);
        size += 2; // for '=' and ','
        i++;
    } while (i < map->size);

    flat_map = (char *)calloc(1, size + 1);
    if (flat_map == NULL)
        return NULL;

    for (i = 0; i < map->size; i++) {
        strcat(flat_map, map->contents[i].key);
        strcat(flat_map, "=");
        strcat(flat_map, map->contents[i].val);
        if ((i + 1) == map->size)
            break;
        strcat(flat_map, ",");
    }

    return flat_map;
}

/* Flattens a CMPIArray of strings into the same string form as above */
char *xen_utils_flatten_CMPIArray(CMPIArray *arr)
{
    int i=0, size=0;
    int elems = CMGetArrayCount(arr, NULL);
    if (elems > 0) {
        do {
            CMPIData data = CMGetArrayElementAt(arr, i, NULL);
            if (!CMIsNullValue(data) && data.type == CMPI_string) {
                size += strlen(CMGetCharPtr(data.value.string));
                size += 1; // for and ','
            }
            i++;
        } while (i < elems);

    }
    if(size == 0) {
        return ""; /* return an empty string since this is used in strcmps */
    }
    char *flat_map = (char *)calloc(1, size + 1);
    if (flat_map == NULL)
        return NULL;
    for (i=0; i< elems; i++) {
        CMPIData data = CMGetArrayElementAt(arr, i, NULL);
        if (data.type == CMPI_string) {
            strcat(flat_map, CMGetCharPtr(data.value.string));
            if (i == elems-1)
                break;
            strcat(flat_map, ",");
        }
    }

    return flat_map;
}

/* Cnoverts a CMPIArray of strings of the form 'key=value'
   into a xen_string_string_map */
xen_string_string_map *
xen_utils_convert_CMPIArray_to_string_string_map(CMPIArray *arr)
{
    xen_string_string_map *map = NULL;
    int i=0;
    int elems = CMGetArrayCount(arr, NULL);
    for (i=0; i<elems; i++) {
        CMPIData data = CMGetArrayElementAt(arr, i, NULL);
        if (!CMIsNullValue(data) && data.type == CMPI_string) {
            char *strcopy = strdup(CMGetCharPtr(data.value.string));
            char *tmp2 =NULL;
            char *key = strtok_r(strcopy, "=", &tmp2);
            char *val = strtok_r(NULL, "=", &tmp2);
            if (key && val)
                xen_utils_add_to_string_string_map(key, val, &map);
            free(strcopy);
        }
    }
    return map;
}

/* converts a string_string_map to a CMPIArray CIM type (of type CMPI_chars) */
CMPIArray *xen_utils_convert_string_string_map_to_CMPIArray(
    const CMPIBroker *broker,
    xen_string_string_map *xen_map
    )
{
    if (xen_map == NULL)
        return NULL;
    CMPIArray *arr = CMNewArray(broker, xen_map->size, CMPI_chars, NULL);
    int i=0;
    for (i=0; i<xen_map->size; i++) {
        char buf[512];
        snprintf(buf, sizeof(buf)/sizeof(buf[0])-1, 
            "%s=%s", 
            xen_map->contents[i].key,
            xen_map->contents[i].val);
        CMSetArrayElementAt(arr, i, (CMPIValue *)buf, CMPI_chars);
    }
    return arr;
}

/*
 * Flatten a Xen API string set into string form "value1,value2.."
 * Caller is responsible for freeing memory.
 */
char *xen_utils_flatten_string_set(
    xen_string_set *set,
    char *delimiter
    )
{
    char *flat_str = NULL;
    int len = 1, i;
    if (!set || set->size == 0)
        return NULL;
    /* start with an empty string */
    flat_str = calloc(1, len);
    *flat_str = '\0';
    for (i=0;i<set->size;i++) {
        if (set->contents[i] && (*(set->contents[i]) != '\0')) {
            /* concatenate all strings here */
            len += strlen(set->contents[i]) + 1;
            flat_str = realloc(flat_str, len);
            strncat(flat_str, set->contents[i], len-1);
            if (i < (set->size-1))
                strncat(flat_str, delimiter, len-1);
            flat_str[len-1] = '\0';
        }
    }
    return flat_str;
}
/*
 * Convert a string with comma delimited entries value1,value2,value3
 * into a string_set
 * Returns pointer to newly constructed string set.
 */
xen_string_set *xen_utils_copy_to_string_set(
    char *str_to_convert,
    char *delimiter
    )
{
    int len = strlen(str_to_convert);
    xen_string_set *strset = NULL;
    int size = 1;
    if (len) {
        /* make a copy of the original string */
        char *str = calloc(1, len+1);
        strncpy(str, str_to_convert, len);

        /* find number of delimiters and alloc new set appropriately */
        char *tmp = strtok(str, delimiter);
        size = 0;
        while (tmp) {
            if (*tmp != '\0')
                size++;
            tmp = strtok(NULL, delimiter);
        }
        strset = xen_string_set_alloc(size);

        /* strtok destroyed the buffer, recreate it */
        free(str);
        str = calloc(1, len+1);
        strncpy(str, str_to_convert, len);

        /* now tokenize and copy substrings to the new set */
        tmp = strtok(str, delimiter);
        size = 0;
        while (tmp) {
            if (*tmp != '\0')
                strset->contents[size++] = strdup(tmp);
            tmp = strtok(NULL, delimiter);
        }
        free(str);
    }
    return strset;
}
/*
 * Add to a string set. Create a new string set if original is empty.
 */
int xen_utils_add_to_string_set(
    char *str,              /* string to add to set */
    xen_string_set **set    /* in/out parameter */
    )
{
    if (*set == NULL) {
        *set = xen_string_set_alloc(1);
        if (*set == NULL)
            return 0;
        (*set)->contents[0] = strdup(str);
        return 1;
    }

    /*  Grow the set and add the new entry */
    int new_len = sizeof(xen_string_set) + 
        (((*set)->size + 1) * sizeof(char *));
    *set = realloc(*set, new_len);
    if (*set == NULL)
        return 0;

    (*set)->contents[(*set)->size] = strdup(str);
    (*set)->size++;
    return 1;
}

/*
 * Convert a string set to a CMPI array
 */
/* converts a string_string_map to a CMPIArray CIM type (of type CMPI_chars) */
CMPIArray *xen_utils_convert_string_set_to_CMPIArray(
    const CMPIBroker *broker,
    xen_string_set *set
    )
{
    if (set == NULL)
        return NULL;
    CMPIArray *arr = CMNewArray(broker, set->size, CMPI_chars, NULL);
    int i=0;
    for (i=0; i<set->size; i++)
        CMSetArrayElementAt(arr, i, (CMPIValue *)set->contents[i], CMPI_chars);
    return arr;
}

/*******************************************************************************
  Miscellaneous utility functions
********************************************************************************/
/* Convert time_t to CMPIDateTime */
signed short CIM_OS_TIMEZONE = 999;
signed short get_os_timezone() {
    if ( CIM_OS_TIMEZONE == 999 ) {
        tzset();
        CIM_OS_TIMEZONE = -(timezone/60);
    }
    return CIM_OS_TIMEZONE;
}

CMPIDateTime* 
    xen_utils_time_t_to_CMPIDateTime(
    const CMPIBroker* broker, 
    time_t time)
{
    CMPIDateTime *cmpi_date_time = NULL;
    CMPIUint64 time_in_mus = time;
    time_in_mus *= 1000000;

    /* time_t is seconds since epoch, on POSIX compliant systems, 
       CMPIDateTime likes it in microsecs  */
    cmpi_date_time = CMNewDateTimeFromBinary(broker, time_in_mus, false, NULL);
    return cmpi_date_time;
}

CMPIDateTime* 
    xen_utils_CMPIDateTime_now(
    const CMPIBroker* broker)
{
    time_t time_now;
    time(&time_now);
    return xen_utils_time_t_to_CMPIDateTime(broker, time_now);
}

time_t
    xen_utils_CMPIDateTime_to_time_t(
    const CMPIBroker* broker, 
    CMPIDateTime *cmpi_time)
{
    if (!cmpi_time)
        return 0;

    CMPIUint64 epoch_time = CMGetBinaryFormat(cmpi_time, NULL);
    if (epoch_time != 0) {
        /* CMPI returns in time since epoch in microsecs, 
           convert to secs, like time_t is expected to be on POSIX compliant systems */
        epoch_time /= 1000000;
    }
    return(time_t) epoch_time;
}
/*
 * Some routines to convert an object path (CIM reference) to its WBEM URI
 * string form:
 * "root/cimv2:ClassName.key1="value",key2="value""
*/
char *xen_utils_CMPIObjectPath_to_WBEM_URI(
    const CMPIBroker *broker, 
    CMPIObjectPath *obj_path
    )
{
    /* edit the string up to the root/cimv2 part */
    return strstr(CMGetCharPtr(CDToString(broker, obj_path, NULL)), 
                  DEFAULT_NS);
}

CMPIObjectPath *xen_utils_WBEM_URI_to_CMPIObjectPath(
    const CMPIBroker *broker, 
    const char* wbem_uri_in /* in the form 'root/cimv2:ClassName.key1="value",key2="value"' */
    )
{
    CMPIObjectPath *obj_path = NULL;
    xen_string_string_map *map = NULL;

    if(!wbem_uri_in)
        return NULL;

    char *classname = NULL;
    char *wbem_uri = strdup(wbem_uri_in);
    int urilen = strlen(wbem_uri);

    char *ns = strtok(wbem_uri, ":"); /* get the namespace */
    if(!ns || (strcmp(ns, DEFAULT_NS) != 0)) {
         /* we may have progressed past the first : and gone to the : in the instance id, 
            reinitialize wbem_uri string */
        /* we are being lenient, accepting URIs that start with classname */
         free(wbem_uri);
         wbem_uri = strdup(wbem_uri_in);
	  // ns is not valid anymore after freeing wbem_uri
	  ns="";
         _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Bad WBEM URI - No namespace %s", wbem_uri));
         classname = wbem_uri; 
    }
    else
        classname = ns + strlen(ns) + 1;
    if(urilen <= (classname - wbem_uri)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Bad WBEM URI - No classname %s", wbem_uri));
        goto Exit;
    }
    classname = strtok(classname, ".");/* move on to the classname */
    if(!classname) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Bad WBEM URI - No classname %s", wbem_uri));
        goto Exit;
    }
    char *key_list = classname + strlen(classname) + 1; /* get the keys */
    if(urilen <= (key_list - wbem_uri)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Bad WBEM URI - No keys %s", wbem_uri));
        goto Exit;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Namespace: %s, classname: %s, Key List %s", ns, classname, key_list));

    /* copy the property list string into a string map for easier access */
    map = xen_utils_convert_string_to_string_map(key_list, ",");
    if(map) {
        obj_path = CMNewObjectPath(broker, DEFAULT_NS, classname, NULL);
        if(!obj_path) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                         ("Bad WBEM URI - Couldnt create ObjectPath with classname %s", wbem_uri));
            goto Exit;
        }
        int i = 0;
        while(i < map->size) {
            CMAddKey(obj_path, map->contents[i].key, map->contents[i].val, CMPI_chars);
            i++;
        }
    }
Exit:
    if(map)
        xen_string_string_map_free(map);
    if(wbem_uri)
        free(wbem_uri);
    return obj_path;
}

/*
 * Trace the error descriptions found in xen session object.
 * This routine uses _sblim_trace function in cmpitrace interface
 * for actual tracing.  Output is to a location specified in the
 * cmpitrace module.
*/
char* xen_utils_get_xen_error(
    xen_session *session
    )
{
    char *tmp = NULL;
    char xen_msg[XEN_UTILS_ERROR_BUF_LEN];
    memset(xen_msg, 0, sizeof(xen_msg));

    /* Always prefix with XenError so its clear its from Xen as opposed to a CIM error */
    if (session && !session->ok && session->error_description) {
        int ndx;
        strcpy(xen_msg, "XenError:");
        for (ndx = 0; ndx < session->error_description_count; ndx++) {
            strncat(xen_msg, session->error_description[ndx],
                XEN_UTILS_ERROR_BUF_LEN - (strlen(xen_msg) + 1));
            if ((ndx+1) != session->error_description_count)
                strncat(xen_msg, ":", XEN_UTILS_ERROR_BUF_LEN - (strlen(xen_msg) + 1));
        }
        tmp = strdup(xen_msg);
    }
    return tmp;
}

void xen_utils_trace_error(
    xen_session *session, 
    char* file, 
    int line)
{
    char *msg = xen_utils_get_xen_error(session);
    if(msg) {
        _sblim_trace(_SBLIM_TRACE_LEVEL_ERROR, file, line, msg);
        free(msg);
    }
}

void xen_utils_set_status(
    const CMPIBroker *broker,
    CMPIStatus *status, 
    int rc, 
    char *default_msg, 
    xen_session *session
    )
{
    char xen_msg[XEN_UTILS_ERROR_BUF_LEN];
    char *xen_err = NULL;
    char *tmp = default_msg;
    memset(xen_msg, 0, sizeof(xen_msg));

    if (rc != 0) { /* All CIM success codes are 0 */
        xen_err = xen_utils_get_xen_error(session);
        if(xen_err)
            tmp = xen_err;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, (default_msg));
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, (tmp));
    }
    else
        tmp = "";
    CMSetStatusWithChars(broker, status, rc, tmp);

    if(xen_err)
        free(xen_err);
}

/*
 * Utility function to parse an input CIM argument of various type
*/
int _GetArgument(
    const CMPIBroker* broker,
    const CMPIArgs* argsin,
    const char* argument_name,
    const CMPIType argument_type,
    CMPIData* argument,
    CMPIStatus* status
    )
{
    *argument = CMGetArg(argsin, argument_name, status);
    if ((status->rc != CMPI_RC_OK) || CMIsNullValue((*argument))) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- CMGetArg %s failed", argument_name));
        return 0;
    }
    if (argument_type == CMPI_ARRAY) {
        if (!CMIsArray((*argument)) || CMIsNullObject(argument->value.array)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- NULL argument %s", argument_name) );
            return 0;
        }
    }
    else if (argument->type != argument_type) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,  ("--- Argument %s of invalid type %s, expecting %s", 
            argument_name, _CMPITypeName(argument->type), _CMPITypeName(argument_type)));
        return 0;
    }
    if ((argument->type == CMPI_ref) && CMIsNullObject(argument->value.ref)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,  ("--- Argument %s is NULL", argument_name));
        return 0;
    }
    return 1;
}

/* Determine the allocation units to use based on the string input.
 * return the units found in bytes or 0 on failure
 */
int64_t xen_utils_get_alloc_units(const char *allocStr)
{
    int64_t retVal = 0;

    if (strstr(allocStr, "Kilo") || strstr(allocStr, "KB") ||
        strstr(allocStr, "2^10")) // using kilobytes
        retVal = 1024;
    else if (strstr(allocStr, "Mega") || strstr(allocStr, "MB") ||
        strstr(allocStr, "2^20")) // using Megabytes
        retVal = 1024 * 1024;
    else if (strstr(allocStr, "Giga") || strstr(allocStr, "GB") ||
        strstr(allocStr, "2^30")) // using Gigabytes
        retVal = 1024 * 1024 * 1024;
    else if (strstr(allocStr, "Byte") || strstr(allocStr, "byte"))
        retVal = 1;
    else if (strstr(allocStr, "Count") || strstr(allocStr, "count"))
        retVal = 1;
    else
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("Unsupported AllocationUnits of %s found", allocStr));
    return retVal;
}

/******************************************************************************
* Helper routines to parse the RASD MOF strings.
* These make use of Lex and Yacc for the parser grammer.
*******************************************************************************/
typedef struct XmlSpecialCharItem {
    const char XmlAscii;
    const char *XmlEscape;
    const int  XmlEscapeSize;
} XmlSpecialChar;

static const XmlSpecialChar XmlEscapes[] = {
    {0x22, "&quot;",   6}, /* '"' */
    {0x26, "&amp;",    5}, /* '&' */
    {0x27, "&apos;",   6}, /* ''' */
    {0x3c, "&lt;", 4}, /* '<' */
    {0x3e, "&gt;", 4}, /* '>' */
};

#define LargestXmlEscapeSize 6 /* From above */
#define SizeofXmlEscapes (sizeof(XmlEscapes)/sizeof(XmlSpecialChar))

char XmlToAscii(const char **XmlStr)
{
    char rval;
    int i;
    if ((rval = *XmlStr[0]) == '&') {
        for (i = 0; i < SizeofXmlEscapes; ++i) {
            if (strncmp(*XmlStr, XmlEscapes[i].XmlEscape,
                XmlEscapes[i].XmlEscapeSize) == 0) {
                *XmlStr += XmlEscapes[i].XmlEscapeSize;
                return XmlEscapes[i].XmlAscii;
            }
        }
    }
    (*XmlStr)++;
    return rval;
}

char * XmlToAsciiStr(const char *XmlStr)
{
    char *AsciiStr = malloc(strlen(XmlStr) + 1); /* assume ascii <= xml len */
    char *Ap = AsciiStr;
    const char *Xp = XmlStr;
    while (*Xp != '\0') {
        *Ap++ = XmlToAscii(&Xp);
    }
    *Ap = '\0';
    return AsciiStr;
}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// THE FOLLOWING CODE IS REQUIRED UNTIL EMBEDDEDOBJECT SUPPORT IS WORKING!!! 
//
int xen_utils_get_cmpi_instance(
    const CMPIBroker *broker,    /* in  */
    CMPIData *setting_data,      /* in  */
    CMPIObjectPath **objectpath, /* out */
    CMPIInstance **instance)     /* out */
{
    CMPIStatus status;
    /* Check that the SettingData is an (embedded) instance */
    if (setting_data->type == CMPI_string) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("cmpi instance is of type string"));

        const char *setting = CMGetCharPtr(setting_data->value.string);
        *instance = xen_utils_parse_embedded_instance(broker, setting);
        if (*instance == NULL) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                ("Could not parse embedded instance %s", setting));
            return 0;
        }
        *objectpath = CMGetObjectPath(*instance, NULL);
    }
    else if (setting_data->type == CMPI_instance) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("cmpi instance is of type instance"));

        *instance = setting_data->value.inst;
        *objectpath = CMGetObjectPath(*instance, NULL);
    }
    else if (setting_data->type == CMPI_ref) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
            ("cmpi instance is of type ref"));

        /*for rasd deletes, the input comes in as a reference*/
        *objectpath = setting_data->value.ref;

	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Object state %d", setting_data->state));
	//_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("object path ok %s", *objectpath));
        *instance = CBGetInstance(broker, NULL, *objectpath, NULL, &status);
	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Got instance"));
	if (status.rc == CMPI_RC_OK) {
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Error: return code is not OK"));
	  _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("%d", status.rc));
	}

	_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("got instance"));
        if (*instance == NULL) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, 
                ("Could not get instance, unknown reference"));
            return 0;
        }
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get instance, unknown type %d", 
            setting_data->type));
        return 0;
    }
    return 1;
}

CMPIInstance *xen_utils_parse_embedded_instance(const CMPIBroker *broker, const char *instanceStr)
{
    char filename[L_tmpnam];
    FILE *fd;
    int rc;
    CMPIInstance *instance;
    char *asciiStr = NULL;

    /* fixup any escaped-XML style string sequences */
    asciiStr = XmlToAsciiStr(instanceStr);

    /* Store the embedded Xen_*SettingData string data in a file for parsing */
    tmpnam(filename);
    fd = fopen(filename, "w");
    fprintf(fd, "%s", asciiStr);
    fclose(fd);
    free(asciiStr);

    /* Parse the embedded Xen_*SettingData string data into a CMPIInstance */
    fd = fopen(filename, "r");
    Xen_SettingDatayyrestart(fd);
    rc = Xen_SettingDatayyparseinstance(broker, &instance);
    fclose(fd);
    remove(filename);
    if (rc != 0) { /* parser returns zero for success, non-zero for error */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- error parsing instance %s", instanceStr));
        return NULL;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,
        ("--- parsed instance: \"%s\"", 
        CMGetCharPtr(CDToString(broker, instance, NULL))));
    return instance;
}

int xen_utils_get_affectedsystem(
    const CMPIBroker *broker,
    xen_utils_session *session,
    const CMPIArgs* argsin,
    CMPIStatus *status,
    xen_vm_record **vm_rec,
    xen_vm *vm
    )
{
    CMPIData argdata;
    int rc=0;

    if (!_GetArgument(broker, argsin, "AffectedSystem", CMPI_ref, &argdata, status))
        if (!_GetArgument(broker, argsin, "AffectedSystem", CMPI_string, &argdata, status))
            goto Exit;
        else {
            const char* domain_uuid = CMGetCharPtr(argdata.value.string);
            if ((strlen(domain_uuid) == 0)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid domain identifier"));
                goto Exit;
            }
            if (!xen_vm_get_by_uuid(session->xen, vm, (char *)domain_uuid)) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not find VM -%s", domain_uuid));
                goto Exit;
            }
            xen_vm_get_record(session->xen, vm_rec, *vm);
            rc=1;
        }
    else {
        if (!xen_utils_get_domain_from_sd_OP(session, vm_rec, vm, argdata.value.ref)) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- xen_vm_get failed"));
        }
        else
            rc = 1;
    }
    Exit:
    if (rc == 0) {
        if (*vm)
            xen_vm_free(*vm);
        if (*vm_rec)
            xen_vm_record_free(*vm_rec);
        *vm=NULL;
        *vm_rec=NULL;
    }
    return rc;
}

int xen_utils_get_vssd_param(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    const CMPIArgs *argsin,
    const char *param_name,
    CMPIStatus *status,
    CMPIInstance **vssd
    )
{
    CMPIData argdata;
    int rc=0;

    if (!_GetArgument(broker, argsin, param_name, CMPI_string, &argdata, status)) {
        if (!_GetArgument(broker, argsin, param_name, CMPI_instance, &argdata, status))
            goto Exit;
        else
            *vssd = argdata.value.inst;
    }
    else {
        const char *vssd_str = CMGetCharPtr(argdata.value.string);
        *vssd = xen_utils_parse_embedded_instance(broker, vssd_str);
        if (*vssd == NULL) { /* parser returns zero for success, non-zero for error */
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- --- Couldnt parse VSSD parameter"));
            status->rc = CMPI_RC_ERR_INVALID_PARAMETER;
            goto Exit;
        }
    }

    rc = 1;

    Exit:
    return rc;
}

bool xen_utils_class_is_subclass_of(
    const CMPIBroker *broker,
    const char *class_to_check, 
    const char *superclass)
{
    CMPIStatus status = {CMPI_RC_OK, NULL};
    CMPIObjectPath * op = CMNewObjectPath(broker, DEFAULT_NS, class_to_check, &status);
    return CMClassPathIsA(broker, op, superclass, &status);
}

/* Routines to parse transfer plugin output */
/*  Parse the transfer record which is in the following xml form
<?xml version="1.0"?>
<transfer_record
  username="70f8e0b297fcdd94"
  status="exposed"
  url_path="/vdi_uuid_4b26aeff-bc12-44f0-a5b6-2e07ec37e75c"
  record_handle="dda7a17f-2621-a77a-0464-6d95a19fbbc3"
  ip="10.80.238.238"
  transfer_mode="bits"
  url_full="http://70f8e0b297fcdd94:8c21f66590fcb08e@10.80.238.238:80/vdi_uuid_4b26aeff-bc12-44f0-a5b6-2e07ec37e75c"
  device="xvdn"
  use_ssl="false"
  password="8c21f66590fcb08e"
  port="80"
  vdi_uuid="4b26aeff-bc12-44f0-a5b6-2e07ec37e75c">
</transfer_record>
    */

char *xen_utils_get_value_from_transfer_record(char *dict, char *key)
{
    char *retval = NULL;
    char buf[128];

    if (!dict || !key || (*dict == '\0'))
        return NULL;

    char *dict_copy = strdup(dict);
    memset(buf, 0, sizeof(buf));
    snprintf(buf, (sizeof(buf)/sizeof(buf[0])), "%s=", key);
    char *val = strstr(dict_copy, buf);
    if (val) {
        /* advance through all whitespaces after the key string */
        val += strlen(buf);
        while (((*val == ' ')||(*val ==':')) && 
            (val < (dict_copy + strlen(dict_copy))))
            val++;

        /* advance through the beginning quote */
        if (*val == '"')
            val++;

        /* find the end quote */
        char *tmp = strstr(val, "\"");
        if (tmp) {
            *tmp = '\0';
            retval = strdup(val);
        }
    }
    free(dict_copy);
    return retval;
}
/*
* Given the transfer plugin's record, gets the URI that the VDI is available on
*/
char *xen_utils_get_uri_from_transfer_record(char *record)
{
    char *uri = NULL;
    if (record && (*record != '\0')) {
        /* Check which protocol we are using */
        char *protocol = xen_utils_get_value_from_transfer_record(record, "transfer_mode");
        if (protocol) {
            if (strcmp(protocol, "iscsi") == 0) {
                /* construct the URI based on the IP, port and IQN */
                char *ip = xen_utils_get_value_from_transfer_record(record, "ip");
                if (ip) {
                    char *port = xen_utils_get_value_from_transfer_record(record, "port");
                    if (port) {
                        char *iscsi_iqn = xen_utils_get_value_from_transfer_record(record, "iscsi_iqn");
                        if (iscsi_iqn) {
                            char *lun = xen_utils_get_value_from_transfer_record(record, "iscsi_lun");
                            if (lun) {
                                #define PROTOCOL "iscsi"
                                int buf_size = strlen(PROTOCOL) + 3 + strlen(ip) + 1 + strlen(port) + 1 + strlen(iscsi_iqn) + strlen(lun) + 1;
                                char *buf = calloc(1, buf_size);
                                //snprintf(buf, (sizeof(buf)/sizeof(buf[0])), "iscsi://%s:%s/%s/lun=%s", ip, port, iscsi_iqn, lun);
                                snprintf(buf, buf_size, "%s://%s:%s/%s", PROTOCOL, ip, port, iscsi_iqn);
                                uri = buf;
                                free(lun);
                            }
                            free(iscsi_iqn);
                        }
                        free(port);
                    }
                    free(ip);
                }
            }
            else {
                /* BITS and HTTP url is the full url from the record */
                char *full_url = xen_utils_get_value_from_transfer_record(record, "url_full");
                if (full_url) {
#if 0
                    /* Add a .vhd to the end of the URL so the disk can be uploaded/downloaded as a VHD file */
                    int urllen = strlen(full_url)+strlen(".vhd")+1;
                    uri = calloc(1, urllen);
                    strncpy(uri, full_url, urllen);
                    strncat(uri, ".vhd", urllen);
                    free(full_url);
#endif
                    uri = full_url;
                }
            }
            free(protocol);
        }
    }
    return uri;
}

xen_string_string_map *
xen_utils_convert_transfer_record_to_string_map(
    char *transfer_record
    )
{
#define BEGIN_TAG "<transfer_record "
#define END_TAG "></transfer_record>"

    char* tr_rec_contents = strstr(transfer_record, BEGIN_TAG);
    if((tr_rec_contents == NULL) || 
       (strlen(tr_rec_contents) <= strlen(BEGIN_TAG))) {
        return NULL;
    }

    tr_rec_contents += strlen(BEGIN_TAG);
    char* end_tag = strstr(tr_rec_contents, END_TAG);
    if(end_tag)
        *end_tag = '\0';

    return xen_utils_convert_string_to_string_map(tr_rec_contents, " ");
}
