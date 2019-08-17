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

int main(int argc, char* argv[])
{
	srand((unsigned int)time(NULL) * 11);
    cout << "\n\nGame Engine! "<<argv[0]<<"\n";
	std::cout.sync_with_stdio();
	auto game = Module::SpawnModule("AsyncGame.exe");

	//auto sb = game.sm.LockFromEngine();
	//sb.Unlock();
	//game.GameLoop();
	run_lib_tests();

	std::cout.sync_with_stdio();
	Sleep(2000);
}
