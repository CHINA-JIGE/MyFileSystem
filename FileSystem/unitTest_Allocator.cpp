#include "Noise3D.h"

using namespace Noise3D::Core;

/*int main()
{
	CAllocator a(10000);

	a.Allocate(0, 100);//boundary
	a.Allocate(100, 400);//left elimate
	a.Allocate(9900, 100);//boundary
	a.Allocate(9500, 400);//right elimate

	a.Allocate(5000, 50);//split
	a.Allocate(5050, 5000);//illegal
	a.Allocate(100, 300);//illegal
	a.Allocate(5050, 4450);//2-side elimate

	a.Release(9950, 50);//boundary
	a.Release(9900, 50);//right merge
	a.Release(0, 50);//boundary
	a.Release(50, 250);//left merge


	a.Release(300, 300);//illegal
	a.Release(3000, 3000);//illegal
	a.Release(6000, 1000);//new free segment
	a.Release(8000, 1000);//new free segment
	a.Release(7000, 1000);//2-side merge


	uint32_t addr1 = a.Allocate(1000);//managed allocation (first fit)
	uint32_t addr2 = a.Allocate(500);
	uint32_t addr3 = a.Allocate(6000);//failed

	return 0;
};*/

int main()
{
	CAllocator a(100);

	a.Allocate(0, 1);
	a.Allocate(1, 1);
	a.Allocate(1, 1);
	a.Allocate(2, 1);
	a.Allocate(3, 1);
	a.Allocate(4, 1);
	a.Allocate(5, 1);
	a.Allocate(6, 1);
	a.Allocate(7, 1);

	a.Allocate(22, 1);
	a.Allocate(23, 1);
	a.Allocate(24, 1);
	a.Allocate(25, 1);
	a.Allocate(26, 1);

	a.Allocate(45, 1);
	a.Allocate(46, 1);

	a.Release(11, 1);
	a.Release(4, 1);
	a.Release(4, 1);
	a.Release(5, 1);
	a.Release(6, 1);
	a.Release(10, 1);
	a.Release(11, 1);

	uint32_t addr1 = a.Allocate(1);//managed allocation (first fit)
	uint32_t addr2 = a.Allocate(1);
	uint32_t addr3 = a.Allocate(1);//failed

	return 0;
};