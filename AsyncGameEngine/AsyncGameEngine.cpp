// AsyncGameEngine.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <time.h>
#include <system_error>
using namespace std;

#include "AsyncGame.h"

class Process
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

public:
	SharedMemory sm;

	Process(wstring path)
	{
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		int shared_mem_id = GetCurrentProcessId();
		sm.CreateSharedMemory(shared_mem_id);

		WCHAR wpath[1024];
		_stprintf_s(wpath, 1024, TEXT("%s %d"), path.c_str(), shared_mem_id);

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
	}

	~Process()
	{

	}

	void Update(SharedMemory &sm)
	{
		//(*(volatile int*)sbf.buff)++;
		//std::cout << (char*)sbt.buff << "\n";

		CommandBlock commands_out;
		Command new_c;
		new_c.command = "sending command!";
		commands_out.commands.push_back(new_c);
		sm.SendCommandsFromEngine(commands_out);

		auto commands_in = sm.GetNextCommandsToEngine();

		if (commands_in.commands.size() == 0) return;
		std::cout << "got " << commands_in.commands.size() << " commands\n";
		for (auto& c : commands_in.commands) {
			std::cout << "\t" << c.command << "\n";
		}
		std::cout << "\n\n";
	}

	void GameLoop()
	{
		while (1) {		
			Update(sm);
			Sleep(10);
		}
	}

};

int main()
{
	srand((unsigned int)time(NULL) * 11);
    cout << "\n\nGame Engine!\n";
	std::cout.sync_with_stdio();
	Process game(TEXT("AsyncGame.exe"));

	//auto sb = game.sm.LockFromEngine();
	//sb.Unlock();
	game.GameLoop();
	
	std::cout.sync_with_stdio();
	Sleep(2000);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
