#include "Utility.h"
#include <stdio.h>

int main(int argc, char**)
{
	int i = 0;
	while(true)
	{
		printf("[%d] exec NewsTaskCreate ...\n", ++i);
		if(!Exec("./NewsTaskCreate ./task/ 1", 8))
			printf("Error: Exec failed!\n");
		if(!DIR_EXIST("./task/.success"))
		{
			printf("Warning: .success not found!\n");
		}
		else
		{
			printf("Info: exec success\n");
			deleteFile("./task/.success");
		}
		Sleep(2);
	}
		
	return 0;
}

