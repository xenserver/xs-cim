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
// Contributors: Adrian Schuur <schuur@de.ibm.com>
// Summary:      SBLIM_TRACE support for SBLIM Project CMPI providers.
// Description:
//    TODO
// For more information about the SBLIM Project see:
//    http://sblim.sourceforge.net/
// ============================================================================

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "cmpitrace.h"

/* Maximum length of trace message text. */
#define _MAXLENGTH 2048

/* By default log all trace messages. */
int _SBLIM_TRACE_LEVEL = _SBLIM_TRACE_LEVEL_ALL;

/* By default log trace messages to stderr. */
static char * _SBLIM_TRACE_FILE = NULL;

char *_sblim_format_trace(char * fmt, ...)
{
    va_list ap;
    char * msg = (char *)malloc(_MAXLENGTH);
    va_start(ap, fmt);
    vsnprintf(msg, _MAXLENGTH, fmt, ap);
    va_end(ap);
    return msg;
}

void _sblim_trace(int level, char *srcfile, int srcline, char *msg)
{
    struct tm cttm;
    struct timeval tv;
    struct timezone tz;
    long sec = 0;
    char * tm = NULL;
    FILE * ferr = stderr;
    int pid;

    static int firsttime = 1;

    /* Initialize _SBLIM_TRACE_LEVEL and _SBLIM_TRACE_FILE from env vars. */
    if (firsttime) {
        char * tracelevel = getenv("SBLIM_TRACE");
        if (tracelevel != NULL) 
            _SBLIM_TRACE_LEVEL = atoi(tracelevel);
        char * tracefile = getenv("SBLIM_TRACE_FILE");
        if (tracefile != NULL) 
            _SBLIM_TRACE_FILE = strdup(tracefile);

        fprintf(ferr, "_SBLIM_TRACE_LEVEL=%x\n", _SBLIM_TRACE_LEVEL);
        fprintf(ferr, "_SBLIM_TRACE_FILE=%s\n", _SBLIM_TRACE_FILE);
        firsttime = 0;
    }

    /* Append this trace message to the existing _SBLIM_TRACE_FILE. */
    if ((_SBLIM_TRACE_FILE != NULL) && (ferr = fopen(_SBLIM_TRACE_FILE, "a")) == NULL) {
        fprintf(stderr, "Cannot open SBLIM_TRACE_FILE %s", _SBLIM_TRACE_FILE);
        return;
    }

    /* Generate a timestamp for this trace message. */
    if (gettimeofday(&tv, &tz) == 0) {
        sec = tv.tv_sec + (tz.tz_minuteswest * -1 * 60);
        tm = (char *) malloc(20 * sizeof(char));
        memset(tm, 0, 20 * sizeof(char));
        if (gmtime_r(&sec, &cttm) != NULL) 
            strftime(tm, 20, "%m/%d/%Y %H:%M:%S", &cttm);
    }

    /* Get PID of the current process. */
    pid = getpid();

    /* Strip off the directory path from the compile-time src filename. */
    if (index(srcfile,'/') != NULL) 
        srcfile = index(srcfile,'/')+1;

    char *level_str;
    switch (level) {
    case _SBLIM_TRACE_LEVEL_ERROR:
        level_str = "ERROR";
        break;
    case _SBLIM_TRACE_LEVEL_WARNING:
        level_str = "WARNING";
        break;
    case _SBLIM_TRACE_LEVEL_INFO:
        level_str = "INFO";
        break;
    case _SBLIM_TRACE_LEVEL_DEBUG:
        level_str = "DEBUG";
        break;
    default:
        level_str = "DEBUG";
    }

    /* Format and print the trace message. */
    fprintf(ferr, "[%s] [%s] %d --- %s(%i) : %s\n", level_str, tm, pid, srcfile, srcline, msg);
    if (tm != NULL) 
        free(tm);

    if ((_SBLIM_TRACE_FILE != NULL)) 
        fclose(ferr);
}

