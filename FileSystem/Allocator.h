
/***********************************************************************

									h£ºAllocator

			Desc: An Index/Address allocator for general use in the
			address space given by the user.
			input an desired size of space, and return a suitable 
			start index/address in given address space, or fail.

************************************************************************/

#pragma once

namespace Noise3D
{
	namespace Core
	{
		struct N_AddressRange
		{
			N_AddressRange(uint32_t _start, uint32_t _size)
			{
				start = _start; 
				size = _size; 
			}
			uint32_t start;
			uint32_t size;
		};

		class /*_declspec(dllexport)*/ CAllocator 
		{
		public:

			CAllocator(uint32_t addressSpaceSize);

			uint32_t	Allocate(uint32_t size);//start address of the allocated segment is decided by allocator,0xffffffff for failure

			bool			Allocate(uint32_t start,uint32_t size);//forcely choose the start address of allocated segment

			bool			Release(uint32_t start,uint32_t size);//return true if the release is legal(no free address is "released")

			bool			IsAddressSpaceRanOut();

			uint32_t	GetFreeSpace();

			uint32_t	GetTotalSpace();

		private:

			uint32_t	mAddressSpaceSize;
			std::list<N_AddressRange>* m_pFreeSegmentList;//list of <start,size>
		};

	}
}