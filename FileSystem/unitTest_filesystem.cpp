#include "Noise3D.h"

using namespace Noise3D::Core;

int main()
{
	IFileSystem fs;
	fs.CreateVirtualDisk("666.nvd", NOISE_VIRTUAL_DISK_CAPACITY_128MB);

	return 0;
};