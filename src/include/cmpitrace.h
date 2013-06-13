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

#if !defined(__CMPITRACE_H__)
#define __CMPITRACE_H__

#include <stdlib.h>

#define _SBLIM_TRACE_LEVEL_ALL     0xff
#define _SBLIM_TRACE_LEVEL_DEBUG   0x04
#define _SBLIM_TRACE_LEVEL_INFO    0x03
#define _SBLIM_TRACE_LEVEL_WARNING 0x02
#define _SBLIM_TRACE_LEVEL_ERROR   0x01


#ifdef SBLIM_DEBUG

/* Setup _SBLIM_TRACE() macros. */

#define _SBLIM_TRACE( LEVEL, STR )                      \
   if ((LEVEL <= _SBLIM_TRACE_LEVEL) && (LEVEL > 0)){   \
      char *msg = _sblim_format_trace STR;              \
      _sblim_trace(LEVEL, __FILE__, __LINE__, msg);     \
      free(msg);                                        \
   }

#define _SBLIM_ENTER( f ) \
   char * __func_ = f; \
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Entering %s()", __func_));

#define _SBLIM_EXIT() { \
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Exiting %s()", __func_)); \
   return; }

#define _SBLIM_RETURN( v ) { \
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Exiting %s()", __func_)); \
   return v; }

#define _SBLIM_RETURNSTATUS( s ) { \
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Exiting %s()=%s", __func_, (s.rc == CMPI_RC_OK)? "OK":"Failed")); \
   return s; }

#define _SBLIM_ABORT() { \
   _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Aborting %s()", __func_)); \
   abort(); } 

#define _SBLIM_TRACE_FUNCTION( LEVEL, f ) \
   if ((LEVEL <= _SBLIM_TRACE_LEVEL) && (LEVEL > 0)) { f; }

#else /* #ifdef SBLIM_DEBUG */

/* Disable _SBLIM_TRACE() macros. */

#define _SBLIM_TRACE( LEVEL, STR )
#define _SBLIM_ENTER( f )
#define _SBLIM_EXIT() {return;}
#define _SBLIM_RETURN( v ) {return v;}
#define _SBLIM_RETURNSTATUS( s ) {return s;}
#define _SBLIM_ABORT() {abort();}
#define _SBLIM_TRACE_FUNCTION( LEVEL, f )
 
#endif /* #ifdef SBLIM_DEBUG */

extern int _SBLIM_TRACE_LEVEL;
extern void _sblim_trace( int level, char * file, int line, char * msg );
extern char * _sblim_format_trace( char * fmt, ... );


#endif /* __CMPITRACE_H__ */
