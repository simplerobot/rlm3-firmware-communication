#include "Test.hpp"
#include "rlm3-fw-communication.h"
#include "rlm3-log-buffer.h"
#include "rlm3-timer.h"
#include "rlm3-memory.h"
#include "rlm3-settings.h"
#include "rlm3-sim.hpp"


static constexpr size_t LOG_BUFFER_SIZE = sizeof(ExternalMemoryLayout::log_buffer);


TEST_CASE(RLM3_FwCommunication_Init_HappyCase)
{
	RLM3_MEMORY_Init();

	RLM3_FwCommunication_Init();

	ASSERT(RLM3_LogBuffer_IsInit());
	ASSERT(RLM3_Timer2_IsInit());

	RLM3_FwCommunication_Deinit();

	ASSERT(!RLM3_LogBuffer_IsInit());
	ASSERT(!RLM3_Timer2_IsInit());
}

TEST_CASE(RLM3_FwCommunication_SendNone)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678;
	SIM_ExpectDebugOutput("");
	RLM3_FwCommunication_Init();

	SIM_DoInterrupt([] { RLM3_Timer2_Event_Callback(); });

	RLM3_FwCommunication_Deinit();
}

TEST_CASE(RLM3_FwCommunication_SendOne)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 1;
	EXTERNAL_MEMORY->log_buffer[0x12345678 % LOG_BUFFER_SIZE] = 'a';
	SIM_ExpectDebugOutput("a");
	RLM3_FwCommunication_Init();

	SIM_DoInterrupt([] { RLM3_Timer2_Event_Callback(); });
	SIM_DoInterrupt([] { RLM3_Timer2_Event_Callback(); });

	RLM3_FwCommunication_Deinit();
}

TEST_CASE(RLM3_FwCommunication_SkipOne)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 2;
	EXTERNAL_MEMORY->log_buffer[0x12345678 % LOG_BUFFER_SIZE] = 'a';
	EXTERNAL_MEMORY->log_buffer[0x12345679 % LOG_BUFFER_SIZE] = 'b';
	SIM_ExpectDebugOutput("b");
	RLM3_FwCommunication_Init();

	EXTERNAL_MEMORY->log_tail++;
	SIM_DoInterrupt([] { RLM3_Timer2_Event_Callback(); });
	SIM_DoInterrupt([] { RLM3_Timer2_Event_Callback(); });

	RLM3_FwCommunication_Deinit();
}

TEST_TEARDOWN(FW_COMM_TEARDOWN)
{
	if (RLM3_Timer2_IsInit())
		RLM3_FwCommunication_Deinit();
}
