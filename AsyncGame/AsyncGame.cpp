#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <string>

#include "../AsyncGameEngine/AsyncGame.h"

int main(int argc, char* argv[])
{
    std::cout << "\n\nGame "<<argv[0]<<" "<<argv[1]<<"!\n";

	int shared_mem_id = 0;
	sscanf_s(argv[1], "%d", &shared_mem_id);

	Sleep(1000);
}

void ModuleMain(LocalModule& lm) {

}