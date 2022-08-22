#pragma once

#include "rlm3-base.h"
#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


extern void RLM3_LogBuffer_Init();
extern void RLM3_LogBuffer_Deinit();
extern bool RLM3_LogBuffer_IsInit();

extern void RLM3_LogBuffer_WriteLogMessage(const char* level, const char* zone, const char* format, va_list params) __attribute__ ((format (printf, 3, 0)));
extern void RLM3_LogBuffer_WriteRawMessage(const char* format, va_list params) __attribute__ ((format (printf, 1, 0)));

extern void RLM3_LogBuffer_FormatLogMessage(const char* level, const char* zone, const char* format, ...) __attribute__ ((format (printf, 3, 4)));
extern void RLM3_LogBuffer_FormatRawMessage(const char* format, ...) __attribute__ ((format (printf, 1, 2)));

extern void RLM3_LogBuffer_DebugChar(const char* channel, char c);

extern uint32_t RLM3_LogBuffer_FetchBlock(size_t max_size);



#ifdef __cplusplus
}
#endif
