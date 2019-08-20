
#include "AsyncGame.h"
using namespace std;

bool HandleMessage(Message& m, MessageBlock &mb) {
	std::cout << "got: " << (char*)m.data << "\n";
	return true;
}

bool Update(MessageBlock& mb) {
	Sleep(300);

	auto m = mb.reserve();
	strcpy_s((char*)m.data, 1024, "ping");
	m.SetDataLen((ushort)strlen((char*)m.data) + 1);
	mb.push(m);
	std::cout << "sending: " << (char*)m.data << "\n";
	return true;
}

int main(int argc, char* argv[])
{
	srand((unsigned int)time(NULL) * 11);
    cout << "\n\nGame Engine! "<<argv[0]<<"\n";
	std::cout.sync_with_stdio();

	SharedMem sm;
	MessageQueue& mq = *CreateSharedMessageQueue(sm, "asynctest");

	auto game = Module::SpawnModule("AsyncGame.exe");

	mq.MessageLoop();

	std::cout.sync_with_stdio();
	Sleep(2000);
}
