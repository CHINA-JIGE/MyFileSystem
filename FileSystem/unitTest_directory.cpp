#include "Noise3D.h"

using namespace Noise3D::Core;

//#define TEST_STAGE_CREATE

#ifdef TEST_STAGE_CREATE
int main()
{
	IFileSystem fs;
	fs.CreateVirtualDisk("666.nvd", NOISE_VIRTUAL_DISK_CAPACITY_128MB);

	return 0;
};

#else

int main()
{
	IFileSystem fs;
	fs.InstallVirtualDisk("666.nvd");


	fs.UninstallVirtualDisk();
	return 0;
};

#endif