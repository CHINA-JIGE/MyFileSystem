
/***********************************************************************

									cpp£ºAllocator

************************************************************************/

#include "Noise3D.h"

using namespace Noise3D::Core;

CAllocator::CAllocator(uint32_t addressSpaceSize)
{
	mAddressSpaceSize = addressSpaceSize;

	//init free segment list with the entire address space
	m_pFreeSegmentList = new std::list<N_AddressRange>;
	m_pFreeSegmentList->push_back(N_AddressRange(0,addressSpaceSize));
}

uint32_t CAllocator::Allocate(uint32_t size)
{
	//range = [start,end)

	//FIRST FIT algorithm
	for (auto pFreeSegIter = m_pFreeSegmentList->begin(); pFreeSegIter != m_pFreeSegmentList->end(); ++pFreeSegIter)
	{
		uint32_t freeSegStart = pFreeSegIter->start;
		uint32_t freeSegEnd = pFreeSegIter->start + pFreeSegIter->size;
		uint32_t allocatedEnd = freeSegStart + size;

		if (pFreeSegIter->size > size)
		{
			//1. first fit match, latter part of this segment still remains free
			*pFreeSegIter = N_AddressRange(allocatedEnd, freeSegEnd - allocatedEnd);
			return freeSegStart;
		}
		else 	if (pFreeSegIter->size == size)
		{
			//2.first fit match, entire segment is allocated
			m_pFreeSegmentList->erase(pFreeSegIter);
			return freeSegStart;
		}
	}

	//failed to allocated
	return c_invalid_alloc_address;
}

bool CAllocator::Allocate(uint32_t start, uint32_t size)
{
	//range = [start,end)
	uint32_t end = start + size;

	for(auto pFreeSegIter =m_pFreeSegmentList->begin();pFreeSegIter!=m_pFreeSegmentList->end();++pFreeSegIter)
	{ 
		// ----------¡¾iterStart----------¡¾start----------end¡¿------------iterEnd¡¿--------
		uint32_t freeSegStart = pFreeSegIter->start;
		uint32_t freeSegEnd = pFreeSegIter->start + pFreeSegIter->size;

		//there are several LEGITIMATE circumstances of allocation
		if (start > freeSegStart && end < freeSegEnd)
		{
			//1.the middle part --- two more free segment is generated
			*pFreeSegIter = N_AddressRange(end, freeSegEnd-end);
			m_pFreeSegmentList->insert (pFreeSegIter, N_AddressRange(freeSegStart, start - freeSegStart));
			return true;
		}
		else 	
		if (start == freeSegStart && end < freeSegEnd)
		{
			//2.the former part is allocated
			*pFreeSegIter = N_AddressRange(end, freeSegEnd-end);
			return true;
		}
		else
		if (start > freeSegStart && end == freeSegEnd)
		{
			//3.the latter part is allocated
			*pFreeSegIter =N_AddressRange(freeSegStart,start- freeSegStart);
			return true;
		}
		else
		if (start == freeSegStart && end == freeSegEnd)
		{
			//4.the whole free segment will be allocated
			m_pFreeSegmentList->erase(pFreeSegIter);
			return true;
		}
		else
		{
			//5,6,7.illegal allocation
		}
	}

	//loop through the free segments list and none is fit
	return false;
}


bool CAllocator::Release(uint32_t start, uint32_t size)
{
	uint32_t end = start + size;

	//insert 2 empty auxiliary segments at boudnary unify the process of merging free segment
	m_pFreeSegmentList->push_front(N_AddressRange(0, 0));
	m_pFreeSegmentList->push_back(N_AddressRange(mAddressSpaceSize, 0));

	//|empty-head|-------|A|------|newly Freed Segment|------------|B|-------|empty-tail|
	auto pFreeSeg1 = m_pFreeSegmentList->begin();
	auto pFreeSeg2 = m_pFreeSegmentList->begin(); pFreeSeg2++;

	//loop through the "Free Segment List"
	while (pFreeSeg2 != m_pFreeSegmentList->end())
	{
		uint32_t seg1End = pFreeSeg1->start + pFreeSeg1->size;
		uint32_t seg2start = pFreeSeg2->start;

		if (start > seg1End && end < seg2start)
		{
			//1. freed segment lies in middle
			m_pFreeSegmentList->insert(pFreeSeg2, N_AddressRange(start, size));

			//delete useless auxiliary free segment
			if (m_pFreeSegmentList->front().size == 0)m_pFreeSegmentList->pop_front();
			if (m_pFreeSegmentList->back().size == 0)m_pFreeSegmentList->pop_back();
			return true;
		}
		else if (start == seg1End && end < seg2start)
		{
			//2. freeSeg1 grows
			pFreeSeg1->size += size;

			//delete useless auxiliary free segment
			if (m_pFreeSegmentList->front().size == 0)m_pFreeSegmentList->pop_front();
			if (m_pFreeSegmentList->back().size == 0)m_pFreeSegmentList->pop_back();
			return true;
		}
		else if (start > seg1End && end == seg2start)
		{
			//3. freeSeg2 grows
			pFreeSeg2->start -= size;
			pFreeSeg2->size += size;

			//delete useless auxiliary free segment
			if (m_pFreeSegmentList->front().size == 0)m_pFreeSegmentList->pop_front();
			if (m_pFreeSegmentList->back().size == 0)m_pFreeSegmentList->pop_back();
			return true;
		}
		else if (start == seg1End && end == seg2start)
		{
			//4, seg1, new seg, seg2 are merged
			pFreeSeg1->size += (size + pFreeSeg2->size);
			m_pFreeSegmentList->erase(pFreeSeg2);

			//delete useless auxiliary free segment
			if (m_pFreeSegmentList->front().size == 0)m_pFreeSegmentList->pop_front();
			if (m_pFreeSegmentList->back().size == 0)m_pFreeSegmentList->pop_back();
			return true;
		}
		
		++pFreeSeg1;
		++pFreeSeg2;
	}

	//delete useless auxiliary free segment
	if (m_pFreeSegmentList->front().size == 0)m_pFreeSegmentList->pop_front();
	if (m_pFreeSegmentList->back().size == 0)m_pFreeSegmentList->pop_back();
	return false;
}

void CAllocator::ReleaseAllSpace()
{
	m_pFreeSegmentList->resize(1, N_AddressRange(0, mAddressSpaceSize));
}

bool CAllocator::IsAddressSpaceRanOut()
{
	return (m_pFreeSegmentList->size()==0);
}

uint32_t CAllocator::GetFreeSpace()
{
	uint32_t fs=0;
	for (auto seg : *m_pFreeSegmentList)fs += seg.size;
	return fs;
}

uint32_t CAllocator::GetTotalSpace()
{
	return mAddressSpaceSize;
}
