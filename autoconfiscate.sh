#!/bin/sh
# ============================================================================
# Copyright (C) 2006 IBM Corporation
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
# Summary:      Setup autoconf/automake build environment for SBLIM providers
# Description:
#    This script sets up this SBLIM provider package so that it can be built
#    using the GNU autoconf/automake/libtool suite across a wide variety of
#    Unix-based operating systems without modification. This script should be
#    run as the first step of building this SBLIM package.
# For more information about the SBLIM Project see:
#    http://sblim.sourceforge.net/
# ============================================================================

# ----------------------------------------------------------------------------
# NO CHANGES SHOULD BE NECESSARY TO THIS FILE
# ----------------------------------------------------------------------------

echo "Running aclocal ..." &&
aclocal --force &&

echo "Running autoheader ..." &&
autoheader --force &&

echo "Running libtool ..." &&
libtoolize --force && 

echo "Running automake ..." &&
automake --add-missing --force-missing &&

echo "Running autoconf ..." &&
autoconf --force &&

echo "You may now run ./configure"

