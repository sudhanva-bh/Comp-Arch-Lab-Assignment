#ifndef SIM_SOFTFLOAT_PLATFORM_H
#define SIM_SOFTFLOAT_PLATFORM_H
#define LITTLEENDIAN 1
#define INLINE static inline
#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif
#endif
