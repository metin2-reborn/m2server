#include <cstdio>

void WriteVersion()
{
	FILE* fp = fopen("version.txt", "w");

	if (fp)
	{
		fprintf(fp, "game revision: 40250");
		fclose(fp);
	}
}

