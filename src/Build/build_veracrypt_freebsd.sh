#!/bin/sh
#
# Copyright (c) 2013-2024 IDRIX
# Governed by the Apache License 2.0 the full text of which is contained
# in the file License.txt included in VeraCrypt binary and source
# code distribution packages.
#

# Absolute path to this script
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in
SCRIPTPATH=$(dirname "$SCRIPT")
# source directory which contains the Makefile
SOURCEPATH=$(readlink -f "$SCRIPTPATH/..")
# directory where the VeraCrypt has been checked out
PARENTDIR=$(readlink -f "$SCRIPTPATH/../../..")

# Make sure only root can run our script
if [ "$(id -u)" != "0" ]; then
   echo "VeraCrypt must be built by root" 1>&2
   exit 1
fi

# the sources of wxWidgets 3.2.5 must be extracted to the parent directory
export WX_ROOT=$PARENTDIR/wxWidgets-3.2.5
echo "Using wxWidgets sources in $WX_ROOT"

cd $SOURCEPATH

echo "Building GUI version of VeraCrypt"

# this will be the temporary wxWidgets directory
export WX_BUILD_DIR=$PARENTDIR/wxBuildGui

gmake WXSTATIC=1 wxbuild && gmake WXSTATIC=1 clean && gmake WXSTATIC=1 && gmake WXSTATIC=1 package

# Uncomment below and comment line above to reuse existing wxWidgets build
#gmake WXSTATIC=1 clean && gmake WXSTATIC=1 && gmake WXSTATIC=1 package

echo "Building console version of VeraCrypt"

# this will be the temporary wxWidgets directory
export WX_BUILD_DIR=$PARENTDIR/wxBuildConsole

gmake WXSTATIC=1 NOGUI=1 wxbuild && gmake WXSTATIC=1 NOGUI=1 clean && gmake WXSTATIC=1 NOGUI=1 && gmake WXSTATIC=1 NOGUI=1 package

# Uncomment below and comment line above to reuse existing wxWidgets build
#gmake WXSTATIC=1 NOGUI=1 clean && gmake WXSTATIC=1 NOGUI=1 && gmake WXSTATIC=1 NOGUI=1 package
