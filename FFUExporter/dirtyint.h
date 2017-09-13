#ifndef _DIRTY_INT_VS2008_H
#define _DIRTY_INT_VS2008_H

#if defined(_MSC_VER) && (_MSC_VER == 1500)
#ifndef uint16_t
typedef unsigned __int16 uint16_t;
typedef __int16 int16_t;
#endif
#ifndef uint8_t
typedef unsigned __int8 uint8_t;
typedef __int8 int8_t;
#endif
#ifndef uint32_t
typedef unsigned __int32 uint32_t;
typedef __int32 int32_t;
#endif

#else
#include <stdint.h>
#endif

#endif