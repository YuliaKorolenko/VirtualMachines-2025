#ifndef LAMA_DEBUG_LOG_H
#define LAMA_DEBUG_LOG_H

#include <stdio.h>


#ifdef DEBUG_OUTPUT
  #define DEBUG_LOG(stream, fmt, ...) fprintf((stream), (fmt), ##__VA_ARGS__)
#else
  #define DEBUG_LOG(stream, fmt, ...) ((void)0)
#endif


#ifdef REWRITE_FPRINTF_TO_DEBUG_LOG
  #undef fprintf
  #define fprintf DEBUG_LOG
#endif

#endif // LAMA_DEBUG_LOG_H
