#include "Test.hpp"
#include "rlm3-log-buffer.h"
#include "rlm3-memory.h"
#include "rlm3-settings.h"
#include "rlm3-task.h"
#include "rlm3-sim.hpp"
#include <cstring>
#include <cstdio>


static constexpr size_t BUFFER_SIZE = sizeof(ExternalMemoryLayout::log_buffer);


TEST_CASE(RLM3_LogBuffer_Lifecycle)
{
	RLM3_MEMORY_Init();
	ASSERT(!RLM3_LogBuffer_IsInit());
	RLM3_LogBuffer_Init();
	ASSERT(RLM3_LogBuffer_IsInit());
	RLM3_LogBuffer_Deinit();
	ASSERT(!RLM3_LogBuffer_IsInit());
}

TEST_CASE(RLM3_LogBuffer_Init_MemoryNotInitialized)
{
	ASSERT_ASSERTS(RLM3_LogBuffer_Init());
}

TEST_CASE(RLM3_LogBuffer_Init_AlreadyInitialized)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();
	ASSERT_ASSERTS(RLM3_LogBuffer_Init());
}

TEST_CASE(RLM3_LogBuffer_Deinit_NeverInitialized)
{
	ASSERT_ASSERTS(RLM3_LogBuffer_Deinit());
}

TEST_CASE(RLM3_LogBuffer_Init_WithValidLog)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + BUFFER_SIZE;

	RLM3_LogBuffer_Init();

	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0x12345678);
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE);
}

TEST_CASE(RLM3_LogBuffer_Init_WithInvalidMagic)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F574D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + BUFFER_SIZE;

	RLM3_LogBuffer_Init();

	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == 0);
}

TEST_CASE(RLM3_LogBuffer_Init_WithInvalidSize)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + BUFFER_SIZE + 1;

	RLM3_LogBuffer_Init();

	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteLogMessage_HappyCase)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();

	RLM3_Delay(30);
	RLM3_LogBuffer_FormatLogMessage("test-level", "test-zone", "test-message %X", 0xACE);

	const char* expected = "L 30 test-level test-zone test-message ACE\n";
	size_t length = std::strlen(expected);
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == length);
	ASSERT(std::strncmp(EXTERNAL_MEMORY->log_buffer, expected, length) == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteLogMessage_FromISR)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();
	RLM3_Delay(30);

	SIM_DoInterrupt([] {
		RLM3_LogBuffer_FormatLogMessage("test-level", "test-zone", "test-message %X", 0xACE);
	});

	const char* expected = "L 30 test-level test-zone test-message ACE\n";
	size_t length = std::strlen(expected);
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == length);
	ASSERT(std::strncmp(EXTERNAL_MEMORY->log_buffer, expected, length) == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteLogMessage_NotInitialized)
{
	RLM3_LogBuffer_FormatLogMessage("test-level", "test-zone", "test-message %X", 0xACE);
}

TEST_CASE(RLM3_LogBuffer_WriteRawMessage_HappyCase)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();

	RLM3_Delay(30);
	RLM3_LogBuffer_FormatRawMessage("test-message %X", 0xACE);

	const char* expected = "test-message ACE\n";
	size_t length = std::strlen(expected);
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == length);
	ASSERT(std::strncmp(EXTERNAL_MEMORY->log_buffer, expected, length) == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteRawMessage_FromISR)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();
	RLM3_Delay(30);

	SIM_DoInterrupt([] {
		RLM3_LogBuffer_FormatRawMessage("test-message %X", 0xACE);
	});

	const char* expected = "test-message ACE\n";
	size_t length = std::strlen(expected);
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == length);
	ASSERT(std::strncmp(EXTERNAL_MEMORY->log_buffer, expected, length) == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteRawMessage_NotInitialized)
{
	RLM3_LogBuffer_FormatRawMessage("test-message %X", 0xACE);
}

TEST_CASE(RLM3_LogBuffer_Init_WithFaultError)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->fault_magic = 0x464F554C;
	std::strncpy(EXTERNAL_MEMORY->fault_cause, "test-fault-cause", sizeof(EXTERNAL_MEMORY->fault_cause));
	std::memcpy(EXTERNAL_MEMORY->fault_communication_thread_state, "test-thread-state", sizeof(EXTERNAL_MEMORY->fault_communication_thread_state));

	RLM3_LogBuffer_Init();

	// For the test, the output will got to the log, but we don't forward the log to our log buffer.
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == 0);
}

TEST_CASE(RLM3_LogBuffer_WriteMessage_MultipleMixed)
{
	RLM3_MEMORY_Init();
	RLM3_LogBuffer_Init();

	RLM3_Delay(30);
	RLM3_LogBuffer_FormatRawMessage("test-message %X", 0xACE);
	RLM3_Delay(30);
	RLM3_LogBuffer_FormatLogMessage("test-level", "test-zone", "test-message %d", 123);
	RLM3_Delay(30);
	RLM3_LogBuffer_FormatRawMessage("test-message %X", 0xACE2);

	const char* expected = "test-message ACE\nL 60 test-level test-zone test-message 123\ntest-message ACE2\n";
	size_t length = std::strlen(expected);
	ASSERT(EXTERNAL_MEMORY->log_magic == 0x4C4F474D);
	ASSERT(EXTERNAL_MEMORY->log_tail == 0);
	ASSERT(EXTERNAL_MEMORY->log_head == length);
	ASSERT(std::strncmp(EXTERNAL_MEMORY->log_buffer, expected, length) == 0);
}

TEST_CASE(RLM3_LogBuffer_FetchBlock_HappyCase)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 2048;
	for (size_t i = 0; i < 2048; i++)
		EXTERNAL_MEMORY->log_buffer[(0x12345678 + i) % BUFFER_SIZE] = (i % 8 == 7) ? '\n' : 'a';
	RLM3_LogBuffer_Init();

	uint32_t end = RLM3_LogBuffer_FetchBlock(1024);

	ASSERT(end == 0x12345678 + 1024);
}

TEST_CASE(RLM3_LogBuffer_FetchBlock_Empty)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678;
	RLM3_LogBuffer_Init();

	uint32_t end = RLM3_LogBuffer_FetchBlock(1024);

	ASSERT(end == 0x12345678);
}

TEST_CASE(RLM3_LogBuffer_FetchBlock_Partial)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 512;
	for (size_t i = 0; i < 512; i++)
		EXTERNAL_MEMORY->log_buffer[(0x12345678 + i) % BUFFER_SIZE] = (i % 8 == 7) ? '\n' : 'a';
	RLM3_LogBuffer_Init();

	uint32_t end = RLM3_LogBuffer_FetchBlock(1024);

	ASSERT(end == 0x12345678 + 512);
}

TEST_CASE(RLM3_LogBuffer_FetchBlock_NewlineBreak)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 512;
	for (size_t i = 0; i < 512; i++)
		EXTERNAL_MEMORY->log_buffer[(0x12345678 + i) % BUFFER_SIZE] = (i % 8 == 7 && i < 408) ? '\n' : 'a';
	RLM3_LogBuffer_Init();

	uint32_t end = RLM3_LogBuffer_FetchBlock(1024);

	ASSERT(end == 0x12345678 + 408);
}

TEST_CASE(RLM3_LogBuffer_FetchBlock_NoNewlines)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + 2048;
	for (size_t i = 0; i < 2048; i++)
		EXTERNAL_MEMORY->log_buffer[(0x12345678 + i) % BUFFER_SIZE] = 'a';
	RLM3_LogBuffer_Init();

	uint32_t end = RLM3_LogBuffer_FetchBlock(1024);

	ASSERT(end == 0x12345678 + 1024);
}

TEST_CASE(RLM3_LogBuffer_Overflow)
{
	RLM3_MEMORY_Init();
	EXTERNAL_MEMORY->log_magic = 0x4C4F474D;
	EXTERNAL_MEMORY->log_tail = 0x12345678;
	EXTERNAL_MEMORY->log_head = 0x12345678 + BUFFER_SIZE - 32;
	for (char& x : EXTERNAL_MEMORY->log_buffer)
		x = 'a';
	RLM3_LogBuffer_Init();

	// Add a log message that fills the buffer.
	RLM3_LogBuffer_FormatRawMessage("12345678901234567890"); // 21 characters.
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE - 11);

	// Try adding another message that is too big for the buffer.  It should fail.
	RLM3_LogBuffer_FormatRawMessage("12345678901"); // 12 characters.
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE - 11);

	// Try adding another message that is not too big for the buffer.  It should fail since the buffer is now locked for overflow.
	RLM3_LogBuffer_FormatRawMessage("123456789"); // 10 characters.
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE - 11);

	// Remove some data from the buffer.
	EXTERNAL_MEMORY->log_tail = RLM3_LogBuffer_FetchBlock(1024);

	// Try adding another message.  It should fail.
	RLM3_LogBuffer_FormatRawMessage("12345678901"); // 12 characters.
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE - 11);

	// Remove half of the data from the buffer.
	for (size_t i = 0; i < BUFFER_SIZE / 2; i += 1024)
		EXTERNAL_MEMORY->log_tail = RLM3_LogBuffer_FetchBlock(1024);

	// Try adding another message.  It should be added.
	RLM3_LogBuffer_FormatRawMessage("12345678901"); // 12 characters.
	ASSERT(EXTERNAL_MEMORY->log_head == 0x12345678 + BUFFER_SIZE - 11 + 12);
}

TEST_TEARDOWN(LOG_BUFFER_TEARDOWN)
{
	if (RLM3_LogBuffer_IsInit())
		RLM3_LogBuffer_Deinit();
}
