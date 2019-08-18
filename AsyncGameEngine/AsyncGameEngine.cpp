#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <time.h>
#include <system_error>
#include <cassert>
#include <assert.h>
using namespace std;

#include "AsyncGame.h"

bool Proc(MessageQueue& mq) {
	static uint cursor = 0;

	mq.Cleanup();

	for (auto recvm : mq.EachMessage(cursor)) {
		std::cout << "got: " << (char*)recvm.data << "\n";
	}
	/*auto recvm = mq.GetBlock(cursor);

	if (recvm) {
		uint i = 0;
		std::cout << "got: " << (char*)recvm->get(i).data << "\n";
	}*/

	MessageBlock mb;

	auto m = mb.reserve();
	strcpy_s((char*)m.data, 1024, "ping");
	m.SetDataLen((ushort)strlen((char*)m.data) + 1);
	mb.push(m);
	std::cout << "sending: " << (char*)m.data << "\n";
	mq.SendMessageBlock(mb.header, mb.data);

	return true;
}

int main(int argc, char* argv[])
{
	run_lib_tests();

	srand((unsigned int)time(NULL) * 11);
    cout << "\n\nGame Engine! "<<argv[0]<<"\n";
	std::cout.sync_with_stdio();

	SharedMem sm;
	MessageQueue& mq = *CreateSharedMessageQueue(sm, "asynctest");

	auto game = Module::SpawnModule("AsyncGame.exe");

	while (Proc(mq)) {
		//Sleep(1);
	}

	std::cout.sync_with_stdio();
	Sleep(2000);
}
