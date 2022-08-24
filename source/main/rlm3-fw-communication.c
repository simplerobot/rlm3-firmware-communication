#include "rlm3-fw-communication.h"
#include "rlm3-base.h"
#include "rlm3-timer.h"
#include "rlm3-log-buffer.h"
#include "rlm3-settings.h"


static const size_t LOG_BUFFER_SIZE = sizeof(ExternalMemoryLayout::log_buffer);

static uint32_t g_debug_console_tail;


extern void RLM3_Timer2_Event_Callback()
{
	// Make sure tail is still a valid reference.
	if (g_debug_console_tail - EXTERNAL_MEMORY->log_tail > LOG_BUFFER_SIZE)
	{
		g_debug_console_tail = EXTERNAL_MEMORY->log_tail;
	}
	// Check if there is any data to send.
	if (EXTERNAL_MEMORY->log_head - g_debug_console_tail > 0)
	{
		char c = EXTERNAL_MEMORY->log_buffer[g_debug_console_tail % LOG_BUFFER_SIZE];
		if (RLM3_DebugOutputFromISR(c))
			g_debug_console_tail++;
	}
}

extern void RLM3_FwCommunication_Init()
{
	RLM3_LogBuffer_Init();

	if (RLM3_IsDebugOutput())
	{
		// When the debugger is connected, we use a timer interrupt to send logs messages to the debug console.
		g_debug_console_tail = EXTERNAL_MEMORY->log_tail;
		RLM3_Timer2_Init(10000);
	}

	// TODO: Start thread to connect to server and serve up a config page.

}

extern void RLM3_FwCommunication_Deinit()
{
	if (RLM3_Timer2_IsInit())
		RLM3_Timer2_Deinit();

	RLM3_LogBuffer_Deinit();
}
