// AsyncGame.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <string>

#include "../AsyncGameEngine/AsyncGame.h"

void Update(SharedMemory &sm)
{
	char buff[1024];
	auto commands_in = sm.GetNextCommandsFromEngine();
	CommandBlock commands_out;
	for(auto &c : commands_in.commands) {
		sprintf_s(buff, 1024, "got %s", c.command.c_str());
		Command new_c;
		new_c.command = buff;
		commands_out.commands.push_back(new_c);
	}
	sm.SendCommandsToEngine(commands_out);
}

int main(int argc, char* argv[])
{
    std::cout << "\n\nGame "<<argv[1]<<"!\n";

	int shared_mem_id = 0;
	sscanf_s(argv[1], "%d", &shared_mem_id);
	SharedMemory sm;
	sm.OpenSharedMemory(shared_mem_id);
	while (1) {
		Update(sm);
		//Sleep(10);
	}
	Sleep(1000);
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
