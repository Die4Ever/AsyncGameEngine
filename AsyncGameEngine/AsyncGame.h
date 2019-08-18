#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <vector>
#include <cassert>
#include <assert.h>
#include <string>
#include <unordered_map>

typedef unsigned __int64 uint64;
typedef unsigned int uint;
typedef unsigned short ushort;
const uint MESSAGE_QUEUE_LEN = 1 * 1024 * 1024;//we'll need to handle this being set by the caller, for compatibility with different versions


struct alignas(8) Uint32Pair {
	uint a;
	uint b;
};

std::vector<wchar_t> CharToWChar(std::string &s)
{
	int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	std::vector<wchar_t> wstr;
	wstr.resize(wlen);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr.data(), wlen);
	return wstr;
}

struct MessageBlockHeader {
	ushort len;
	std::atomic<ushort> read_count;
	uint sender_id;

	MessageBlockHeader() : read_count(0) {
		len = sizeof(MessageBlockHeader);
		sender_id = 0;
	}
};

struct MessageHeader {
	ushort type;//maybe this type should be extremely generic and part of the protocol? like { Base, KeyValue, JSON, Sender, Targeted, SenderAndTarget, MultiTarget }
	ushort len;

	MessageHeader() {
		type = 0;
		len = 0;
	}
};

class Message {
public:
	MessageHeader header;
	byte* data;

	Message(byte *d) : data(d) {

	}
};

class MessageBlock {
public:
	MessageBlockHeader header;
	byte data[64 * 1024];
private:
	//ushort start;

public:

	Message get(uint &pos) {
		assert(pos + sizeof(MessageBlockHeader) < header.len);
		Message m(data + pos + sizeof(MessageHeader));
		m.header = *(MessageHeader*)(data + pos);
		return m;
	}

	void push(Message m) {
		MessageHeader &inplace = *(MessageHeader*)(data + header.len - sizeof(MessageBlockHeader));
		inplace = m.header;
		assert(m.data == data + header.len + sizeof(MessageHeader) - sizeof(MessageBlockHeader));
		header.len += m.header.len;
	}

	Message reserve() {
		Message m(data + header.len + sizeof(MessageHeader) - sizeof(MessageBlockHeader));
		return m;
	}

};

class MessageQueue {
	//circular buffer? can I make it loop to the beginning early if the queue has room at the front in order to improve cache use? basically a dynamically sized circular buffer
	//the hub module would just reset end to 0 when appropriate? but that means readers will need to check both if the next spot is a new message and if the start is a new message

	//uint protocol_version;?
	//std::atomic<uint> end;
	//std::atomic<uint> start;
	std::atomic<Uint32Pair> len_end;
	byte data[MESSAGE_QUEUE_LEN * 2];//double length so we don't need to strictly wrap based on the length of the MessageBlock being added, we just wrap by the start point

	uint _ReserveSpace(uint len) {
		auto old_len_end = len_end.load(std::memory_order::memory_order_relaxed);
		auto new_len_end = old_len_end;

		do {
			new_len_end = old_len_end;
			new_len_end.a += len;
			new_len_end.b = (new_len_end.b + len) % MESSAGE_QUEUE_LEN;
		} while (!len_end.compare_exchange_weak(old_len_end, new_len_end));

		while (len_end.load(std::memory_order::memory_order_relaxed).a > MESSAGE_QUEUE_LEN) {
			std::cout << "message queue is full! waiting...\n";
		}

		return old_len_end.b;
	}

	bool _IsInRange(uint pos) {
		assert(pos < MESSAGE_QUEUE_LEN);
		auto old_len_end = len_end.load(std::memory_order::memory_order_relaxed);
		uint len = old_len_end.a;
		uint end = old_len_end.b;
		uint start = (end - len) % MESSAGE_QUEUE_LEN;
		bool is_after_start = int(pos) >= int(end) - int(len);//treat the wrap of the circular buffer as a negative number
		bool is_before_end = pos < start + len;//check the end position without modulus
		return is_after_start && is_before_end;
	}

public:
	MessageQueue() {
		assert(len_end.is_lock_free());
		auto t = len_end.load();
		t.a = 0;
		t.b = 0;
		len_end = t;
		MessageBlockHeader mb;
		assert(mb.read_count.is_lock_free());
		memset(data, 0, sizeof(data));
	}

	void SendMessageBlock(MessageBlockHeader &block, byte* mdata)
	{
		//assert the lengths add up
		//assert the types of the messages are recognized?
		const auto old_end = _ReserveSpace(block.len);
		
		memcpy(data + old_end, &block, sizeof(block));
		memcpy(data + old_end + sizeof(block), mdata, block.len);
		MessageBlockHeader* written_block = (MessageBlockHeader*)(data + old_end);
		written_block->read_count.store(1, std::memory_order::memory_order_release);
	}

	MessageBlock* GetBlock(uint &pos)
	{
		//should I return in-place or a copy? I wonder if in-place will cause issues with atmoic variable updates having to propogate to the caches on other cores
		//pretty sure incrementing the atomic read_count will invalidate that whole cache line for every other cache
		//maybe a future optimization could be have a separate array for read_counts to keep them in a different cache line from the data
		//I should benchmark all the options with many processes doing pings
		
		if (!_IsInRange(pos))
			return NULL;

		MessageBlock* block = (MessageBlock*)(data + pos);
		ushort read_count = block->header.read_count.load(std::memory_order::memory_order_relaxed);
		if (read_count == 0)
			return NULL;//read_count of 0 means the message is still being written
		pos = (pos + block->header.len) % MESSAGE_QUEUE_LEN;
		return block;
	}
};

class SharedMem {
	static void* MapFile(HANDLE hMapFile, int BUF_SIZE)
	{
		void* pBuf = MapViewOfFile(hMapFile,   // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,
			0,
			BUF_SIZE);

		if (pBuf == NULL)
		{
			CloseHandle(hMapFile);

			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "Could not map view of file");
		}
		return pBuf;
	}

	HANDLE hMap;
	void* pBuf;

public:

	void* CreateSharedMemory(std::string path, HANDLE& hMapFile, void*& pBuf, uint BUF_SIZE)
	{
		std::cout << "creating shared memory " << path << "\n";

		auto wstr = CharToWChar(path);

		hMapFile = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			BUF_SIZE,                // maximum object size (low-order DWORD)
			wstr.data());            // name of mapping object

		if (hMapFile == NULL)
		{
			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "Could not create file mapping object");
		}
		pBuf = MapFile(hMapFile, BUF_SIZE);

		//new (pBuf) std::atomic<int>(0);

		std::cout << "shared memory " << path << " created!\n";
		return pBuf;
	}

	void* OpenSharedMemory(std::string path, HANDLE& hMapFile, void*& pBuf, uint BUF_SIZE)
	{
		std::cout << "opening shared memory " << path << "\n";

		auto wstr = CharToWChar(path);

		hMapFile = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			wstr.data());               // name of mapping object

		if (hMapFile == NULL)
		{
			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "Could not create file mapping object");
		}
		pBuf = MapFile(hMapFile, BUF_SIZE);

		std::cout << "shared memory " << path << " opened!\n";
		return pBuf;
	}

	void* CreateSharedMemory(std::string path, uint BUF_SIZE)
	{
		return CreateSharedMemory(path, hMap, pBuf, BUF_SIZE);
	}

	void* OpenSharedMemory(std::string path, uint BUF_SIZE)
	{
		return OpenSharedMemory(path, hMap, pBuf, BUF_SIZE);
	}

	~SharedMem()
	{
		assert(pBuf != NULL);
		assert(hMap != NULL);
		UnmapViewOfFile(pBuf);
		CloseHandle(hMap);
		pBuf = NULL;
		hMap = NULL;
		std::cout << "shared memory destroyed!";
	}
};

class Module {
public:
	uint id;
	PROCESS_INFORMATION pi;

	static Module SpawnModule(std::string name) {
		Module spawned = Module();
		uint shared_mem_id = GetCurrentProcessId();
		uint pid = spawned.SpawnProcess(name, shared_mem_id);
		return spawned;
	}

	uint SpawnProcess(std::string path, uint shared_mem_id) {
		STARTUPINFO si;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		auto wstr = CharToWChar(path);
		WCHAR wpath[4096];
		_stprintf_s(wpath, 4096, TEXT("%s %u"), wstr.data(), shared_mem_id);

		bool ret = CreateProcess(NULL,   // No module name (use command line)
			wpath,        // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi);           // Pointer to PROCESS_INFORMATION structure

		if (!ret) {
			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "error spawning process");
		}
		return pi.dwProcessId;
	}
};

class LocalModule : public Module {
	//hold data like where we are in the message queue, event listeners and callbacks? performance stats?
};

void _ParseKeyValue(const char*& c, std::unordered_map<std::string, std::string> &map) {
	const char* eq = strchr(c, '=') + 1;
	const char* newline = strchr(c, '\n');
	const char* cr = strchr(c, '\r');
	const char* end = newline;
	if (cr && cr < end) end = cr;
	if (end == NULL) end = eq + strlen(eq);
	std::string k(c, eq-c-1);
	std::string v(eq, end - eq);
	c = newline;
	map[k] = v;
	c = end;
}

std::unordered_map<std::string, std::string> ParseKeyValues(std::string text) {
	std::unordered_map<std::string, std::string> map;
	const char* c = text.c_str();
	while (*c) {
		if (*c != '#') {
			_ParseKeyValue(c, map);
		}
		while (*c) {
			c++;
			if (c[-1] == '\n') break;
		}
	}
	return map;
}

void run_lib_tests() {
	auto map = ParseKeyValues("foo=bar\n#test comment\nsigma=nuts");
	assert(map.size() == 2);
	assert(map["foo"] == "bar");
	assert(map["sigma"] == "nuts");

	std::atomic<Uint32Pair> t2;
	assert(t2.is_lock_free());
	assert(sizeof(Uint32Pair) == 8);
	assert(sizeof(t2) == sizeof(Uint32Pair));

	SharedMem sm;
	void* d = sm.CreateSharedMemory("foobar", sizeof(MessageQueue));
	MessageQueue& mq = *new (d) MessageQueue();

	MessageBlock mb;
	auto m = mb.reserve();
	strcpy_s((char*)m.data, 1024, "foobar");
	m.header.len = strlen((char*)m.data) + sizeof(m.header);
	mb.push(m);

	mq.SendMessageBlock(mb.header, mb.data);

	uint cursor = 0;
	auto& recv_mb = *mq.GetBlock(cursor);
	cursor = 0;
	m = recv_mb.get(cursor);
	assert(strcmp((char*)m.data, "foobar") == 0);
	std::cout << "got: " << (char*)m.data << "\n";
}
