
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

