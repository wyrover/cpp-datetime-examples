#include <Windows.h>
#include <Mmsystem.h>
#include <iostream>


#include <stddef.h>                     // for size_t, NULL
#include <stdlib.h>                     // for malloc, free, realloc
#include <stdio.h>
#include <set>                          // for set, etc

#include "base/logging.h"               // for operator<<, CHECK, etc

using std::set;



int main(int argc, char** argv) {
	

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	LARGE_INTEGER start, end;
	QueryPerformanceCounter(&start);

	int i = 0;
	const int ARRAY_SIZE = 100000;
	int* ar_pi[ARRAY_SIZE] = { nullptr, };

	std::cout << "[== tc malloc ==]\n";

	

	for (i = 0; i<ARRAY_SIZE; ++i)
	{
		ar_pi[i] = new int[0x1000];
		ar_pi[i][0] = 0x124f;
	}

	for (i = 0; i<ARRAY_SIZE; ++i)
	{
		delete[] ar_pi[i];
		ar_pi[i] = nullptr;
	}


	QueryPerformanceCounter(&end);



	std::cout << (end.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart << "ms" << std::endl;
	return 0;
}
