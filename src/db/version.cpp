#include <cstdio>
#include <cstdlib>

void WriteVersion()
{
	FILE* fp(fopen("VERSION.txt", "w"));

	if (nullptr != fp)
	{
		fprintf(fp, "db revision: %s\n", __P4_VERSION__);
		fclose(fp);
	}
	else
	{
		fprintf(stderr, "cannot open VERSION.txt\n");
		exit(0);
	}
}

