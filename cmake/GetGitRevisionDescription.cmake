# GetGitRevisionDescription.cmake
#
# This file is part of CMake.
#
# Copyright (c) 2010 Kitware, Inc. Copyright (c) 2010 Eike Ziller
# <eike.ziller@qt.io>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

set(HEAD_HASH)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD HEAD_CONTENTS LIMIT 1024)

string(STRIP "${HEAD_CONTENTS}" HEAD_CONTENTS)
if(HEAD_CONTENTS MATCHES "ref")
  # named branch
  string(REPLACE "ref: " "" HEAD_REF "${HEAD_CONTENTS}")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/${HEAD_REF}")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.git/${HEAD_REF}" HEAD_HASH
         LIMIT 1024)
    string(STRIP "${HEAD_HASH}" HEAD_HASH)
  endif()
else()
  # detached HEAD
  string(STRIP "${HEAD_CONTENTS}" HEAD_HASH)
endif()

if(NOT HEAD_HASH)
  set(HEAD_HASH "unknown")
endif()
