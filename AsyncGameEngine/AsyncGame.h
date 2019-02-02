#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <vector>

class SharedBuffer
{
private:
	//lock

	std::atomic<int>* _get_mutex()
	{
		std::atomic<int>* p = (std::atomic<int>*)buff;
		p--;
		return p;

		/*std::mutex* p = (std::mutex*)buff;
		p--;*/
		//std::mutex& m = *p;
		return p;
	}

	void _lock()
	{
		auto p = _get_mutex();
		while (p->exchange(1) != 0) {
			//lock using the PID instead of the number 1, then I can make it reentrant
			//I'll need to switch the atomic function to do a compare and swap
			//if debug, keep a counter and warn on long waits
		}
	}

	void _unlock()
	{
		auto p = _get_mutex();
		p->exchange(0);
	}

	void* buff;
public:
	/*SharedBuffer(SharedBuffer&& sb)
	{
		buff = sb.buff;
		sb.buff = NULL;
	}

	SharedBuffer(SharedBuffer& sb)
	{
		buff = sb.buff;
		sb.buff = NULL;
	}*/

	SharedBuffer(void* b)
	{
		//lock
		std::atomic<int>* m = (std::atomic<int>*)b;
		buff = (void*)(m + 1);
		_lock();
	}

	void Unlock()
	{
		if (buff == NULL) return;

		_unlock();
		buff = NULL;
	}

	~SharedBuffer()
	{
		if (buff == NULL) return;
		std::cerr << "you need to release the SharedBuffer and swap\n";
		Unlock();
		//unlock
		//flip buffer? or maybe make it explicit?
	}

	/*operator void*()
	{
		return buff;
	}*/

	unsigned int GetSize()
	{
		return *(unsigned int*)buff;
	}

	void SetSize(unsigned int size)
	{
		*(unsigned int*)buff = size;
	}

	byte* GetBytes()
	{
		return (byte*)buff + sizeof(unsigned int);
	}
};

class Command
{
public:
	std::string command;
};

class CommandBlock
{
public:
	//build up a list of commands to send, on destructor copy them to the shared memory
	std::vector<Command> commands;
};


class SharedMemory
{
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

	HANDLE hMapToEngine[2];
	void* pBufToEngine[2];

	HANDLE hMapFromEngine[2];
	void* pBufFromEngine[2];

	int ToEngineBackBuffer;
	int FromEngineBackBuffer;

public:

	void CreateSharedMemory(TCHAR *name, HANDLE &hMapFile, void* &pBuf)
	{
		const int BUF_SIZE = 4 * 1024 * 1024;
		std::wcout << "creating shared memory " << std::wstring(name) << "\n";

		hMapFile = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			BUF_SIZE,                // maximum object size (low-order DWORD)
			name);                   // name of mapping object

		if (hMapFile == NULL)
		{
			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "Could not create file mapping object");
		}
		pBuf = MapFile(hMapFile, BUF_SIZE);

		//new (pBuf) std::mutex();
		new (pBuf) std::atomic<int>(0);

		std::wcout << "shared memory " << std::wstring(name) << " created!\n";
	}

	void CreateSharedMemory(int shared_mem_id)
	{
		TCHAR name[1024];
		_stprintf_s(name, 1024, TEXT("AsyncGame-ToEngine0-%d"), shared_mem_id);
		CreateSharedMemory(name, hMapToEngine[0], pBufToEngine[0]);
		_stprintf_s(name, 1024, TEXT("AsyncGame-ToEngine1-%d"), shared_mem_id);
		CreateSharedMemory(name, hMapToEngine[1], pBufToEngine[1]);

		_stprintf_s(name, 1024, TEXT("AsyncGame-FromEngine0-%d"), shared_mem_id);
		CreateSharedMemory(name, hMapFromEngine[0], pBufFromEngine[0]);
		_stprintf_s(name, 1024, TEXT("AsyncGame-FromEngine1-%d"), shared_mem_id);
		CreateSharedMemory(name, hMapFromEngine[1], pBufFromEngine[1]);

		ToEngineBackBuffer = 0;
		FromEngineBackBuffer = 0;
	}

	void OpenSharedMemory(TCHAR *name, HANDLE& hMapFile, void* &pBuf)
	{
		const int BUF_SIZE = 4 * 1024 * 1024;
		std::wcout << "opening shared memory " << std::wstring(name) << "\n";

		hMapFile = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			name);               // name of mapping object

		if (hMapFile == NULL)
		{
			throw std::system_error(std::error_code((int)GetLastError(), std::system_category()), "Could not create file mapping object");
		}
		pBuf = MapFile(hMapFile, BUF_SIZE);

		std::wcout << "shared memory " << std::wstring(name) << " opened!\n";
	}

	void OpenSharedMemory(int shared_mem_id)
	{
		TCHAR name[1024];
		_stprintf_s(name, 1024, TEXT("AsyncGameEngine-%d"), shared_mem_id);
		_stprintf_s(name, 1024, TEXT("AsyncGame-ToEngine0-%d"), shared_mem_id);
		OpenSharedMemory(name, hMapToEngine[0], pBufToEngine[0]);
		_stprintf_s(name, 1024, TEXT("AsyncGame-ToEngine1-%d"), shared_mem_id);
		OpenSharedMemory(name, hMapToEngine[1], pBufToEngine[1]);

		_stprintf_s(name, 1024, TEXT("AsyncGame-FromEngine0-%d"), shared_mem_id);
		OpenSharedMemory(name, hMapFromEngine[0], pBufFromEngine[0]);
		_stprintf_s(name, 1024, TEXT("AsyncGame-FromEngine1-%d"), shared_mem_id);
		OpenSharedMemory(name, hMapFromEngine[1], pBufFromEngine[1]);

		ToEngineBackBuffer = 0;
		FromEngineBackBuffer = 0;
	}

	SharedBuffer LockToEngine()
	{
		return SharedBuffer(pBufToEngine[ToEngineBackBuffer]);
	}

	SharedBuffer LockFromEngine()
	{
		return SharedBuffer(pBufFromEngine[FromEngineBackBuffer]);
	}

	void UnlockToEngine(SharedBuffer& sb)
	{
		sb.Unlock();
		//ToEngineBackBuffer = (ToEngineBackBuffer + 1) % 2;//need to figure out when to swap buffers and how to keep the reader and writer on opposite buffers and keep the commands in order?
	}

	void UnlockFromEngine(SharedBuffer& sb)
	{
		sb.Unlock();
		//FromEngineBackBuffer = (FromEngineBackBuffer + 1) % 2;
	}

	CommandBlock GetNextCommands(SharedBuffer& sb)
	{
		int bytes = 0;
		int total_bytes = sb.GetSize();
		byte* buff = sb.GetBytes();
		byte* end = buff + total_bytes;

		CommandBlock block;

		while (buff < end) {
			unsigned int size = *(unsigned int*)buff;
			buff += sizeof(unsigned int);
			std::string command((char*)buff, size);
			Command c;
			c.command = command;
			block.commands.push_back(c);
			buff += size;
		}

		sb.SetSize(0);
		return block;
	}

	CommandBlock GetNextCommandsToEngine()
	{
		auto sb = LockToEngine();
		auto c = GetNextCommands(sb);
		UnlockToEngine(sb);
		return c;
	}

	CommandBlock GetNextCommandsFromEngine()
	{
		auto sb = LockFromEngine();
		auto c = GetNextCommands(sb);
		UnlockFromEngine(sb);
		return c;
	}

	void SendCommands(SharedBuffer &sb, CommandBlock &cb)
	{
		int bytes = sb.GetSize();
		byte* buff = sb.GetBytes();
		buff += bytes;

		for (auto& c : cb.commands) {
			unsigned int len = (unsigned int)c.command.length();
			*(unsigned int*)buff = len;
			bytes += sizeof(unsigned int);
			buff += sizeof(unsigned int);
			memcpy(buff, c.command.c_str(), len);
			bytes += len;
			buff += len;
		}

		sb.SetSize(bytes);
	}

	void SendCommandsToEngine(CommandBlock& cb)
	{
		if (cb.commands.size() == 0) return;
		auto sb = LockToEngine();
		SendCommands(sb, cb);
		UnlockToEngine(sb);
	}

	void SendCommandsFromEngine(CommandBlock& cb)
	{
		if (cb.commands.size() == 0) return;
		auto sb = LockFromEngine();
		SendCommands(sb, cb);
		UnlockFromEngine(sb);
	}

	~SharedMemory()
	{
		UnmapViewOfFile(pBufToEngine[0]);
		UnmapViewOfFile(pBufToEngine[1]);
		UnmapViewOfFile(pBufFromEngine[0]);
		UnmapViewOfFile(pBufFromEngine[1]);
		CloseHandle(hMapToEngine[0]);
		CloseHandle(hMapToEngine[1]);
		CloseHandle(hMapFromEngine[0]);
		CloseHandle(hMapFromEngine[1]);

		std::cout << "shared memory destroyed!";
	}
};
