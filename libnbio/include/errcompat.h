/*
 * libnbio - Portable wrappers for non-blocking sockets
 * Copyright (c) 2000-2005 Adam Fritzler <mid@zigamorph.net>, et al
 *
 * libnbio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (version 2.1) as published by
 * the Free Software Foundation.
 *
 * libnbio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __ERRCOMPAT_H__
#define __ERRCOMPAT_H__

/* Just for sanity's sake, we define these with the "standard" UNIX meanings */
#define EINTR 4
#define EAGAIN 11
#define EFAULT 14
#define EINVAL 22
#define ENOTSOCK 88
#define EMSGSIZE 90
#define EOPNOTSUPP 95
#define ENETDOWN 100
#define ENETRESET 102
#define ECONNABORTED 103
#define ECONNRESET 104
#define ENOTCONN 107
#define ESHUTDOWN 108
#define ETIMEDOUT 110
#define EINPROGRESS 115

#endif /* def __ERRCOMPAT_H__ */

