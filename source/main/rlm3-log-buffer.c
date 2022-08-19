#include "rlm3-log-buffer.h"
#include "logger.h"
#include "Assert.h"
#include "rlm3-base.h"
#include "rlm3-settings.h"
#include "rlm3-task.h"
#include "rlm3-lock.h"
#include "rlm3-string.h"


#define LOG_MAGIC (0x4C4F474D) // 'LOGM'
#define FAULT_MAGIC (0x464F554C) // 'FOUL'


LOGGER_ZONE(LOG_BUFFER);


static RLM3_MutexLock g_lock;
static volatile bool g_is_initialized = false;
static size_t g_debug_output_cursor;
static volatile size_t g_log_allocation_head;
static volatile size_t g_active_logger_count = 0;


static void FormatToBufferFn(void* data, char c)
{
	size_t* cursor = (size_t*)data;
	EXTERNAL_MEMORY->log_buffer[(*cursor)++ % sizeof(EXTERNAL_MEMORY->log_buffer)] = c;
}

static void FormatToDebugOutput(void* data, char c)
{
	RLM3_DebugOutput(c);
}

static uint32_t EnterCritical()
{
	uint32_t result = 0;
	if (RLM3_IsIRQ())
		result = RLM3_EnterCriticalFromISR();
	else
		RLM3_EnterCritical();
	return result;
}

static void ExitCritical(uint32_t saved_level)
{
	if (RLM3_IsIRQ())
		RLM3_ExitCriticalFromISR(saved_level);
	else
		RLM3_ExitCritical();
}

static bool BeginOutputToBuffer(size_t size, size_t* offset_out)
{
	ExternalMemoryLayout* external_memory = (ExternalMemoryLayout*)RLM3_EXTERNAL_MEMORY_ADDRESS;

	bool result = false;
	uint32_t saved_level = EnterCritical();
	size_t available_size = sizeof(external_memory->log_buffer) - (g_log_allocation_head - external_memory->log_tail);
	if (size <= available_size)
	{
		size_t head = g_log_allocation_head;
		*offset_out = head;
		g_log_allocation_head = head + size;
		g_active_logger_count++;
		result = true;
	}
	ExitCritical(saved_level);
	return result;
}

static void EndOutputToBuffer()
{
	ASSERT(g_active_logger_count != 0);

	ExternalMemoryLayout* external_memory = (ExternalMemoryLayout*)RLM3_EXTERNAL_MEMORY_ADDRESS;

	uint32_t saved_level = EnterCritical();
	if (--g_active_logger_count == 0)
		external_memory->log_head = g_log_allocation_head;
	ExitCritical(saved_level);
}

extern void RLM3_LogBuffer_Init()
{
	ASSERT(RLM3_MEMORY_IsInit());
	ASSERT(!g_is_initialized);

	RLM3_MutexLock_Init(&g_lock);

	// If the current log information in the external memory is not valid, reset it.
	ExternalMemoryLayout* external_memory = (ExternalMemoryLayout*)RLM3_EXTERNAL_MEMORY_ADDRESS;
	if (external_memory->log_magic != LOG_MAGIC || external_memory->log_head - external_memory->log_tail > sizeof(external_memory->log_buffer))
	{
		external_memory->log_head = 0;
		external_memory->log_tail = 0;
	}
	external_memory->log_magic = LOG_MAGIC;
	g_debug_output_cursor = external_memory->log_tail;
	g_log_allocation_head = external_memory->log_head;

	if (external_memory->fault_magic == FAULT_MAGIC)
	{
		external_memory->fault_magic = 0;
		external_memory->fault_cause[sizeof(external_memory->fault_cause) - 1] = 0;
		external_memory->fault_communication_thread_state[sizeof(external_memory->fault_communication_thread_state) - 1] = 0;
		LOG_FATAL("Forced Restart: '%s' COMM: %s", external_memory->fault_cause, external_memory->fault_communication_thread_state);
	}

	g_is_initialized = true;
}

extern void RLM3_LogBuffer_Deinit()
{
	ASSERT(g_is_initialized);

	RLM3_MutexLock_Deinit(&g_lock);

	g_is_initialized = false;
}

extern bool RLM3_LogBuffer_IsInit()
{
	return g_is_initialized;
}

extern void RLM3_LogBuffer_WriteLogMessage(const char* level, const char* zone, const char* format, va_list params)
{
	if (!g_is_initialized)
	{
		// Initialization is not complete, so messages cannot be stored.  Write them directly to the debug port.
		RLM3_FnFormat(FormatToDebugOutput, NULL, "L 0 %s %s ", level, zone);
		RLM3_FnVFormat(FormatToDebugOutput, NULL, format, params);
		RLM3_DebugOutput('\n');
		return;
	}

	bool is_irq = RLM3_IsIRQ();

	// TODO: use a time offset to convert tick_count to a time with ms.
	RLM3_Time tick_count = (is_irq ? RLM3_GetCurrentTimeFromISR() : RLM3_GetCurrentTime());

	// Determine the size of this log message.
	va_list args;
	va_copy(args, params);
	size_t header_size = RLM3_FormatNoNul(NULL, 0, "L %u %s %s ", (int)tick_count, level, zone);
	size_t content_size = RLM3_VFormatNoNul(NULL, 0, format, args);
	size_t total_size = header_size + content_size + 1;
	va_end(args);

	if (!is_irq)
		RLM3_MutexLock_Enter(&g_lock);

	// Allocate buffer
	size_t offset;
	if (BeginOutputToBuffer(total_size, &offset))
	{
		// Write this log message into the buffer
		RLM3_FnFormat(FormatToBufferFn, &offset, "L %u %s %s ", (int)tick_count, level, zone);
		RLM3_FnVFormat(FormatToBufferFn, &offset, format, params);
		FormatToBufferFn(&offset, '\n');
		EndOutputToBuffer();
	}

	if (!is_irq)
		RLM3_MutexLock_Leave(&g_lock);
}

extern void RLM3_LogBuffer_WriteRawMessage(const char* format, va_list params)
{
	if (!g_is_initialized)
	{
		// Initialization is not complete, so messages cannot be stored.  Write them directly to the debug port.
		RLM3_FnVFormat(FormatToDebugOutput, NULL, format, params);
		RLM3_DebugOutput('\n');
		return;
	}

	bool is_irq = RLM3_IsIRQ();

	// Determine the size of this log message.
	va_list args;
	va_copy(args, params);
	size_t content_size = RLM3_VFormatNoNul(NULL, 0, format, args);
	size_t total_size = content_size + 1;
	va_end(args);

	if (!is_irq)
		RLM3_MutexLock_Enter(&g_lock);

	// Allocate buffer
	size_t offset;
	if (BeginOutputToBuffer(total_size, &offset))
	{
		// Write this log message into the buffer
		RLM3_FnVFormat(FormatToBufferFn, &offset, format, params);
		FormatToBufferFn(&offset, '\n');
		EndOutputToBuffer();
	}

	if (!is_irq)
		RLM3_MutexLock_Leave(&g_lock);
}

extern void RLM3_LogBuffer_FormatLogMessage(const char* level, const char* zone, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	RLM3_LogBuffer_WriteLogMessage(level, zone, format, args);
	va_end(args);
}

extern void RLM3_LogBuffer_FormatRawMessage(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	RLM3_LogBuffer_WriteRawMessage(format, args);
	va_end(args);
}
