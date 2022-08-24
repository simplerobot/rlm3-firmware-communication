#include "rlm3-log-buffer.h"
#include "logger.h"
#include "Assert.h"
#include "rlm3-base.h"
#include "rlm3-settings.h"
#include "rlm3-task.h"
#include "rlm3-lock.h"
#include "rlm3-string.h"
#include <string.h>


#define LOG_MAGIC (0x4C4F474D) // 'LOGM'
#define FAULT_MAGIC (0x464F554C) // 'FOUL'

static const size_t BUFFER_SIZE = sizeof(ExternalMemoryLayout::log_buffer);
static const size_t FULL_BUFFER_RESTART_LIMIT = BUFFER_SIZE / 2;


LOGGER_ZONE(LOG_BUFFER);


static RLM3_MutexLock g_lock;
static volatile bool g_is_initialized = false;
static volatile bool g_is_overflow = false;
static volatile size_t g_log_allocation_head;
static volatile size_t g_active_logger_count = 0;

static const char* g_debug_channel = NULL;


static void FormatToBufferFn(void* data, char c)
{
	size_t* cursor = (size_t*)data;
	EXTERNAL_MEMORY->log_buffer[(*cursor)++ % BUFFER_SIZE] = c;
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
	size_t available_size = BUFFER_SIZE - (g_log_allocation_head - external_memory->log_tail);
	if (size > available_size)
	{
		g_is_overflow = true;
	}
	else if (!g_is_overflow)
	{
		size_t head = g_log_allocation_head;
		*offset_out = head;
		g_log_allocation_head = head + size;
		g_debug_channel = NULL;
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
	g_debug_channel = NULL;
	ExitCritical(saved_level);
}

extern void RLM3_LogBuffer_Init()
{
	ASSERT(RLM3_MEMORY_IsInit());
	ASSERT(!g_is_initialized);

	RLM3_MutexLock_Init(&g_lock);

	// If the current log information in the external memory is not valid, reset it.
	ExternalMemoryLayout* external_memory = (ExternalMemoryLayout*)RLM3_EXTERNAL_MEMORY_ADDRESS;
	if (external_memory->log_magic != LOG_MAGIC || external_memory->log_head - external_memory->log_tail > BUFFER_SIZE)
	{
		external_memory->log_head = 0;
		external_memory->log_tail = 0;
	}
	external_memory->log_magic = LOG_MAGIC;
	g_log_allocation_head = external_memory->log_head;
	g_debug_channel = NULL;
	g_is_overflow = false;

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

extern uint32_t RLM3_LogBuffer_FetchBlock(size_t max_size)
{
	ASSERT(g_is_initialized);
	uint32_t head = EXTERNAL_MEMORY->log_head;
	uint32_t tail = EXTERNAL_MEMORY->log_tail;
	// If the buffer gets full, we wait until it is half empty to add anything else in it.  This ensures we have reasonably coherent logs.
	if (g_is_overflow && head - tail < FULL_BUFFER_RESTART_LIMIT)
	{
		g_is_overflow = false;
		LOG_ALWAYS("Overflow");
	}
	// Get the largest chunk of the buffer that is available.
	uint32_t target = head;
	if (target - tail > max_size)
		target = tail + max_size;
	// Try to reduce it so the block ends with a newline.
	for (uint32_t i = 0; i < target - tail; i++)
		if (EXTERNAL_MEMORY->log_buffer[(target - i - 1) % BUFFER_SIZE] == '\n')
			return target - i;
	// The block does not have a newline to break on.
	return target;
}

extern void RLM3_LogBuffer_DebugChar(const char* channel, char c)
{
	ASSERT(channel != NULL);

	if (!g_is_initialized)
	{
		RLM3_DebugOutput(c);
		return;
	}

	ExternalMemoryLayout* external_memory = (ExternalMemoryLayout*)RLM3_EXTERNAL_MEMORY_ADDRESS;

	uint32_t saved_level = EnterCritical();
	if (c == '\n' || c == '\r')
	{
		// End any previous debug character message.
		if (g_active_logger_count == 0)
			external_memory->log_head = g_log_allocation_head;
		g_debug_channel = NULL;
	}
	else
	{
		// Make sure our character is printable.
		if (c < ' ' || c > '~')
			c = '?';

		if (channel != g_debug_channel)
		{
			// We are starting a new channel, so add a new header.
			size_t available_size = BUFFER_SIZE - (g_log_allocation_head - external_memory->log_tail);
			size_t header_size = strlen(channel) + 5; // Output: "D CHANNEL C\n"
			if (header_size <= available_size && !g_is_overflow)
			{
				// End the previous debug character message.
				if (g_active_logger_count == 0)
					external_memory->log_head = g_log_allocation_head;

				// Allocate space for this header.
				g_debug_channel = channel;
				size_t head = g_log_allocation_head;
				g_log_allocation_head = head + header_size;

				// Write this initial message into the buffer.
				FormatToBufferFn(&head, 'D');
				FormatToBufferFn(&head, ' ');
				for (size_t i = 0; channel[i] != 0; i++)
					FormatToBufferFn(&head, channel[i]);
				FormatToBufferFn(&head, ' ');
				FormatToBufferFn(&head, c);
				FormatToBufferFn(&head, '\n');

				ASSERT(head == g_log_allocation_head);
			}
		}
		else
		{
			// We are already writing to this channel, so just allocate one additional character.
			size_t available_size = BUFFER_SIZE - (g_log_allocation_head - external_memory->log_tail);
			if (1 <= available_size && !g_is_overflow)
			{
				// Replace the \n that is currently at the end of this log message with the new character and add one more character.
				g_debug_channel = channel;
				size_t head = g_log_allocation_head - 1;
				g_log_allocation_head = head + 2;

				// Write this character to the buffer.
				FormatToBufferFn(&head, c);
				FormatToBufferFn(&head, '\n');

				ASSERT(head == g_log_allocation_head);
			}
		}
	}
	ExitCritical(saved_level);
}
