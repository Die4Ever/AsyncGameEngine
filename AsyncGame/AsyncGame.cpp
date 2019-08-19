#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <string>

#include "../AsyncGameEngine/AsyncGame.h"

bool Proc(MessageQueue& mq) {
	static uint cursor = mq.GetStart();
	MessageBlock mb;

	for(auto recvm : mq.EachMessage(cursor)) {
		for (int i = 0; i < 10 && mb.header.len < 10000; i++) {
			auto m = mb.reserve();
			strcpy_s((char*)m.data, 1024, "foobar");
			m.SetDataLen((ushort)strlen((char*)m.data) + 1);
			mb.push(m);
		}
		//don't send from inside the loop, because the message hasn't been marked as read yet, I need a better abstraction for this
	}

	if(mb.header.len > sizeof(mb.header))
		mq.SendMessageBlock(mb.header, mb.data);
	return true;
}

int main(int argc, char* argv[])
{
    std::cout << "\n\nGame "<<argv[0]<<" "<<argv[1]<<"!\n";

	int shared_mem_id = 0;
	sscanf_s(argv[1], "%d", &shared_mem_id);

	SharedMem sm;
	MessageQueue& mq = *OpenSharedMessageQueue(sm, "asynctest", shared_mem_id);
	Sleep(2000);
	while (Proc(mq)) {
		Sleep(1);
	}

	Sleep(1000);
}
