
/* shim file for musl */

#ifndef __MUSL_SHIM_BITS_ALLTYPES_H__
#define __MUSL_SHIM_BITS_ALLTYPES_H__

#define _REDIR_TIME64 1
#define _Addr int
#define _Int64 long long
#define _Reg int

#define __BYTE_ORDER 1234

#define __LONG_MAX 0x7fffffffL

#ifndef __cplusplus
typedef unsigned wchar_t;
#endif

typedef float float_t;
typedef double double_t;

#endif /* __MUSL_SHIM_BITS_ALLTYPES_H__ */
