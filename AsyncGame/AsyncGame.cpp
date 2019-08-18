#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <string>

#include "../AsyncGameEngine/AsyncGame.h"

bool Proc(MessageQueue& mq) {
	static uint cursor = 0;
	auto recvm = mq.GetBlock(cursor);

	if (recvm) {
		MessageBlock mb;

		for (int i = 0; i < 10; i++) {
			auto m = mb.reserve();
			strcpy_s((char*)m.data, 1024, "foobar");
			m.SetDataLen((ushort)strlen((char*)m.data) + 1);
			mb.push(m);
		}

		mq.SendMessageBlock(mb.header, mb.data);
	}

	return true;
}

int main(int argc, char* argv[])
{
    std::cout << "\n\nGame "<<argv[0]<<" "<<argv[1]<<"!\n";

	int shared_mem_id = 0;
	sscanf_s(argv[1], "%d", &shared_mem_id);

	SharedMem sm;
	MessageQueue& mq = *OpenSharedMessageQueue(sm, "asynctest", shared_mem_id);

	while (Proc(mq)) {
		//Sleep(1);
	}

	Sleep(1000);
}
