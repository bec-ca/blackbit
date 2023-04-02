#pragma once

namespace blackbit {

#ifdef _DEBUG
#define SUICIDE()                                                              \
  {                                                                            \
    int* __i__ = 0;                                                            \
    int __v__ = *__i__;                                                        \
    *__i__ = __v__;                                                            \
  }
#define ASSERT(VALUE)                                                          \
  while (not(VALUE)) { SUICIDE(); }
#else
#define SUICIDE()
#define ASSERT(VALUE)
#endif

#ifdef _NOINLINE
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

} // namespace blackbit
