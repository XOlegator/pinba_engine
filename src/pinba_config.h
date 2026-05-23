/* Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef PINBA_ENGINE_PINBA_CONFIG_H
#define PINBA_ENGINE_PINBA_CONFIG_H

/* CMake generates build/src/config.h; include it via build root include path. */
#include "src/config.h"

#ifdef DEBUG_ON
#define PINBA_ENGINE_DEBUG_ON DEBUG_ON
#endif

#ifdef DEBUG_OFF
#define PINBA_ENGINE_DEBUG_OFF DEBUG_OFF
#endif

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
#define PINBA_ENGINE_HAVE_PTHREAD_SETAFFINITY_NP HAVE_PTHREAD_SETAFFINITY_NP
#endif

#ifdef HAVE_SYSCONF
#define PINBA_ENGINE_HAVE_SYSCONF HAVE_SYSCONF
#endif

#ifdef HAVE_STRNDUP
#define PINBA_ENGINE_HAVE_STRNDUP HAVE_STRNDUP
#else
#define PINBA_ENGINE_HAVE_STRNDUP 0
#endif

#ifdef HAVE_RECVMMSG
#define PINBA_ENGINE_HAVE_RECVMMSG HAVE_RECVMMSG
#else
#if defined(__linux__)
#define PINBA_ENGINE_HAVE_RECVMMSG 1
#else
#define PINBA_ENGINE_HAVE_RECVMMSG 0
#endif
#endif

#ifdef PINBA_BUILD_STRING
#define PINBA_ENGINE_PINBA_BUILD_STRING PINBA_BUILD_STRING
#endif

#ifdef VCS_DATE
#define PINBA_ENGINE_VCS_DATE VCS_DATE
#endif

#ifdef VCS_BRANCH
#define PINBA_ENGINE_VCS_BRANCH VCS_BRANCH
#endif

#ifdef VCS_FULL_HASH
#define PINBA_ENGINE_VCS_FULL_HASH VCS_FULL_HASH
#endif

#ifdef VCS_SHORT_HASH
#define PINBA_ENGINE_VCS_SHORT_HASH VCS_SHORT_HASH
#endif

#ifdef VCS_WC_MODIFIED
#define PINBA_ENGINE_VCS_WC_MODIFIED VCS_WC_MODIFIED
#endif

#endif /* PINBA_ENGINE_PINBA_CONFIG_H */
