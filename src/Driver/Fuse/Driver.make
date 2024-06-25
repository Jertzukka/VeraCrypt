#
# Derived from source code of TrueCrypt 7.1a, which is
# Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
# by the TrueCrypt License 3.0.
#
# Modifications and additions to the original source code (contained in this file)
# and all other portions of this file are Copyright (c) 2013-2017 IDRIX
# and are governed by the Apache License 2.0 the full text of which is
# contained in the file License.txt included in VeraCrypt binary and source
# code distribution packages.
#

NAME := Driver

OBJS :=
OBJS += FuseService.o

if [ -d "$(shell $(PKG_CONFIG) $(VC_FUSE_PACKAGE) --cflags)" ]; then \
  CXXFLAGS += $(shell $(PKG_CONFIG) $(VC_FUSE_PACKAGE) --cflags) \
else \
  CXXFLAGS += -I/Library/Application\ Support/fuse-t/include/fuse \
fi

include $(BUILD_INC)/Makefile.inc
