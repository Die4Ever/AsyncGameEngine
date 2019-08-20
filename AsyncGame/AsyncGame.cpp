
#include "../AsyncGameEngine/AsyncGame.h"

bool HandleMessage(Message& m, MessageBlock& mb) {
	auto m2 = mb.reserve();
	strcpy_s((char*)m2.data, 1024, "foobar");
	m2.SetDataLen((ushort)strlen((char*)m2.data) + 1);
	mb.push(m2);
	return true;
}

bool Update(MessageBlock& mb) {
	Sleep(1);
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
	mq.MessageLoop();

	Sleep(1000);
}
