# ============================================================================
# (C) Copyright IBM Corp. 2006
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Lesser General Public
#    License as published by the Free Software Foundation; either
#    version 2.1 of the License, or (at your option) any later version.
#
#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public
#    License along with this library; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# ============================================================================
# Author:       Dr. Gareth S. Bestor <bestor@us.ibm.com>
# Contributors: Viktor Mihajlovski <mihajlov@de.ibm.com>
#               Jim Fehlig <jfehlig@novell.com>
#		Luke Szymanski <Lukasz.Szymanski@unisys.com>
#               Shashi Uli <shashi.uli@citrix.com>
# Summary:      Autoconf input file for CMPI Xen Provider
# Description:
#    This file is processed by autoconf to generate the configure script for
#    building this SBLIM provider. This file must be customized for every
#    SBLIM provider package.
# For more information about the SBLIM Project see:
#    http://sblim.sourceforge.net/
# ============================================================================

# ----------------------------------------------------------------------------
# PROVIDER SPECIFIC CONFIGURATION VALUES
# ----------------------------------------------------------------------------

# Package information
# CHANGE THIS TO THE PACKAGE SUMMARY INFORMATION
# AC_INIT(<PACKAGE_NAME>, <PACKAGE_VERSION>, <PACKAGE_BUGREPORT>, <PACKAGE_TARBALL>)
AC_INIT(XenServer CMPI Provider, 1, , xs-cim-cmpi)

# Identify the source directory 
# CHANGE THIS TO THE RELATIVE PATHNAME OF *ONE* OF THE SOURCE FILES
AC_CONFIG_SRCDIR([src/Xen_ComputerSystem.c])

# Version number for any versioned provider libraries
PROVIDER_CURRENT_VERSION=1
PROVIDER_REVISION_VERSION=0
PROVIDER_AGE_VERSION=0
PROVIDER_VERSION_INFO=$PROVIDER_CURRENT_VERSION:$PROVIDER_REVISION_VERSION:$PROVIDER_AGE_VERSION

AC_SUBST([PROVIDER_VERSION_INFO])


# Check for required programs, such as commands used in system() calls or
# any special tools required for building this package (e.g. lex, yacc, etc)
# ADD CHECKS FOR ANY REQUIRED PROGRAMS HERE
#AC_CHECK_PROG(PS,xm,[xm list])

# Check for required header files
# ADD CHECKS FOR ANY REQUIRED NON-CMPI HEADER FILES HERE
AC_CHECK_HEADERS([stdarg.h errno.h])

# Check for required library functions
# ADD CHECKS FOR ANY REQUIRED NON_CMPI LIBRARY FUNCTIONS HERE
AC_CHECK_FUNCS([popen pclose fgets vfprintf fprintf sprintf sscanf])

# Is pkg-config available?
AC_CHECK_PROG([HAVE_PKG_CONFIG], [pkg-config], [yes], [no])

# Check for libxen
AC_ARG_WITH([libxenserver], [  --with-libxenserver=PREFIX    libxenserver location])
AC_CHECK_LIB([xenserver], [xen_init], 
             [LIBXEN_LIBS="-L$with_libxenserver -lxenserver"
	      LIBXEN_CFLAGS="-I$with_libxenserver/include"],
	     [AC_MSG_ERROR([xen library not found])],
	     [-lxml2])

AC_SUBST([LIBXEN_LIBS])
AC_SUBST([LIBXEN_CFLAGS])

# Add argument for specifying location of libxml2
# NOTE: libxenapi uses libxml2 but does not link with it.
#       Ensure libxml2 is available.
AC_ARG_WITH([libxml], [  --with-libxml=PREFIX    libxml2 location])

# Ensure we have libxml2.  Use pkg-config 
LIBXML2_MIN_VERSION=2.5.0
LIBXML2_CONFIG="xml2-config"
LIBXML2_CFLAGS=""
LIBXML2_LIBS=""
LIBXML2_FOUND="no"

if test "x$with_libxml" = "xno" ; then
    AC_MSG_CHECKING([for libxml2 libraries >= $LIBXML2_MIN_VERSION]) 
    AC_MSG_ERROR(libxml2 >= $LIBXML2_MIN_VERSION is required for this package)
elif test "x$with_libxml" = "x" -a "x$HAVE_PKG_CONFIG" = "xyes" ; then
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/lib/pkgconfig:/usr/local/lib/pkgconfig"
    PKG_CHECK_MODULES([LIBXML2], [libxml-2.0 >= $LIBXML2_MIN_VERSION],
                      [LIBXML2_FOUND=yes],
                      [LIBXML2_FOUND=no])
fi

if test "x$LIBXML2_FOUND" = "xno" ; then
    AC_MSG_CHECKING([for libxml2 libraries >= $LIBXML2_MIN_VERSION]) 
    if test "x$with_libxml" != "x" ; then 
	LIBXML2_CONFIG=$with_libxml/bin/$LIBXML2_CONFIG
    fi
    if ! $LIBXML2_CONFIG --version > /dev/null 2>&1 ; then
	AC_MSG_ERROR([Could not find libxml2 anywhere (see config.log for details).])
    fi
    vers=`$LIBXML2_CONFIG --version | awk -F. '{ printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
    minvers=`echo $LIBXML2_MIN_VERSION | awk -F. '{ printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
    if test "$vers" -ge "$minvers" ; then
        LIBXML2_LIBS="`$LIBXML2_CONFIG --libs`"
        LIBXML2_CFLAGS="`$LIBXML2_CONFIG --cflags`"
        AC_MSG_RESULT([yes])
    else
        AC_MSG_ERROR([You need at least libxml2 $LIBXML2_MIN_VERSION])
    fi
fi

AC_SUBST([LIBXML2_CFLAGS])
AC_SUBST([LIBXML2_LIBS])


# Add argument to allow specifying which host instrumentation
# to use.  omc or sblim are the options, omc is default.
HOST_INSTRUMENTATION="omc"
AC_ARG_WITH([host-instrumentation],
	[  --with-host-instrumentation=TYPE  supported types are omc and sblim],
	[
		if test "x$withval" != "xno" ; then
			
			HOST_INSTRUMENTATION="$withval" 
		fi
	]
)

if test "x$HOST_INSTRUMENTATION" = "xomc" ; then
	HOST_INSTRUMENTATION_NS="smash"
	HOST_INSTRUMENTATION_PREFIX="OMC_"
elif test "x$HOST_INSTRUMENTATION" = "xsblim" ; then
	HOST_INSTRUMENTATION_NS="root/cimv2"
	HOST_INSTRUMENTATION_PREFIX="Linux__"
else
	AC_MSG_ERROR([unrecognized host instrumentation $HOST_INSTRUMENTATION])
fi

AC_SUBST([HOST_INSTRUMENTATION_NS])
AC_SUBST([HOST_INSTRUMENTATION_PREFIX])

# Set compiler flag for CMPI API version compatibility
# SET THE PACKAGE CMPI_VERSION HERE
#CPPFLAGS="$CPPFLAGS -DCMPI_VERSION=100 -DCMPI_VER_90 -DCMPI_VER_86 -DCMPI_VER_100 -DCMPI_PLATFORM_LINUX_GENERIC_GNU -DXENAPI_VERSION=500"
CPPFLAGS="$CPPFLAGS -DXENAPI_VERSION=500"

# By default compile in SBLIM_TRACE support
# REMOVE THIS FLAG TO DISABLE SBLIM_TRACE SUPPORT IN THE PROVIDER
#CPPFLAGS="$CPPFLAGS -DSBLIM_DEBUG"


################### NO CHANGES SHOULD BE NECESSARY BELOW ######################

# Configure command line options
AC_ARG_VAR([CIMSERVER],[the target CIM server (pegasus|sfcb|openwbem|sniacimom).])
AC_ARG_VAR([PROVIDERDIR],[the directory where the CMPI providers will be installed.])
AC_ARG_VAR([TESTSUITEDIR],[the directory where the SBLIM testsuite is installed.])
AC_ARG_VAR([XMTESTDIR],[the directory where xm-test is installed.])

# Autogenerate the autoconf header file to store build settings
AC_CONFIG_HEADER([config.h])

# Autogenerate the Makefiles
AC_CONFIG_FILES([Makefile
		 src/Makefile])

AM_INIT_AUTOMAKE([-Wall -Werror])

# Check for the required C compiler
AC_PROG_CC

# Check if the C compiler supports 'const'
AC_C_CONST

# Check for the required libtool
AC_PROG_LIBTOOL

# Check for the required install program 
AC_PROG_INSTALL

AC_PROG_YACC
AC_PROG_LEX

# Check for some common required headers
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Check if the system headers conform to ANSI C
AC_HEADER_STDC

# Check for the required CMPI header files (this macro is defined in acinclude.m4)
CHECK_CMPI

# Check for where to install CMPI libraries (this macro is defined in acinclude.m4)
CHECK_PROVIDERDIR

# Check for presense of a CIM server (this macro is defined in acinclude.m4)
CHECK_CIMSERVER

# Check for the SBLIM testsuite (this macro is defined in acinclude.m4)
CHECK_TESTSUITE
if test x"$TESTSUITEDIR" == x; then
   TESTSUITE_SUPPORT=Disabled
else
   TESTSUITE_SUPPORT=Enabled
fi

AM_CONDITIONAL([CIM_IS_OPENPEGASUS], [test x$CIMSERVER = xpegasus])

# Display configuration options
echo "----------------------------------------------------------"
echo "Configuration for $PACKAGE complete."
echo ""
echo "The following configuration options have been selected:"
echo "   CIMSERVER: " $CIMSERVER
echo "   PROVIDERDIR: " $PROVIDERDIR
echo "   XMTESTDIR: " $XMTESTDIR
echo "   TESTSUITE_SUPPORT: " $TESTSUITE_SUPPORT
if test x"$TESTSUITEDIR" != x; then
   echo "   TESTSUITEDIR: " $TESTSUITEDIR
fi
echo "   HOST_INSTRUMENTATION: " $HOST_INSTRUMENTATION
echo "   CPPFLAGS:" $CPPFLAGS
echo
echo "You can override these values by setting the corresponding"
echo "configure command-line option(s); e.g."
echo "   ./configure PROVIDERDIR=/home/me/lib/sblim/"
echo "----------------------------------------------------------"

# Generate configure scripts for the Makefile
AC_OUTPUT

echo "You may now run make"

