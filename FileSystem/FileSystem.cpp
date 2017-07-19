
/***********************************************************************

									cpp£ºFile System

************************************************************************/

#include "Noise3D.h"

using namespace Noise3D::Core;

/***************************************************
								IFILE
****************************************************/

IFile::IFile() :
	mIsFileNeedUpdate(false)
{

}

IFile::~IFile()
{
	delete m_pFileBuffer;
}

UINT IFile::GetFileSize()
{
	return mFileByteSize;
}

const char * IFile::GetFileData()
{
	return m_pFileBuffer;
}

void IFile::Write(char * pSrcData, uint32_t startIndex, uint32_t size)
{
	if (startIndex + size <= mFileByteSize)
	{
		uint32_t remainSize = size - startIndex;
		//copy 
		memcpy_s(m_pFileBuffer+startIndex, remainSize, pSrcData, remainSize);
	}
	else
	{
		DEBUG_MSG("IFile : 'Write' failure! Index out of boundary!");
	}
}



/**************************************************************

									FILE SYSTEM

***************************************************************/

IFileSystem::IFileSystem():
	IFactory<IFile>(131072),
	m_pVirtualDisk(nullptr),
	m_pFileAddressAllocator(nullptr),
	m_pIndexNodeAllocator(nullptr),
	mIsVDiskInitialized(false),
	mLoggedInAccountID(0xffff),
	mVDiskCapacity(0),
	mVDiskFreeSpace(0),
	mVDiskUserFileOffset(0)
{
}

IFileSystem::~IFileSystem()
{
	if (mIsVDiskInitialized)UninstallVirtualDisk();
}

bool IFileSystem::CreateVirtualDisk(NFilePath filePath, NOISE_VIRTUAL_DISK_CAPACITY cap)
{
	std::ofstream outFile(filePath.c_str(),std::ios::binary);
	if (!outFile.is_open())
	{
		DEBUG_MSG("FileSystem: Create virtual disk failed! file cannot be created.");
		return false;
	}

	//select Virtual Disk capacity
	N_VirtualDiskHeaderInfo headerInfo;
	switch (cap)
	{
	default:
	case NOISE_VIRTUAL_DISK_CAPACITY_128MB:
		headerInfo.diskCapacity = 128 * 1024 * 1024;
		headerInfo.indexNodeCount = 16384;
		break;
	case NOISE_VIRTUAL_DISK_CAPACITY_256MB:
		headerInfo.diskCapacity = 256 * 1024 * 1024;
		headerInfo.indexNodeCount = 32768;
		break;
	case NOISE_VIRTUAL_DISK_CAPACITY_512MB:
		headerInfo.diskCapacity = 512 * 1024 * 1024;
		headerInfo.indexNodeCount = 65536;
		break;
	case NOISE_VIRTUAL_DISK_CAPACITY_1GB:
		headerInfo.diskCapacity = 1024 * 1024 * 1024;
		headerInfo.indexNodeCount = 131072;
		break;
	}
	//disk header length (size of inode table included)
	headerInfo.diskHeaderLength = sizeof(N_VirtualDiskHeaderInfo) + headerInfo.indexNodeCount * sizeof(N_IndexNode);

	//header info
	outFile.write((char*)&headerInfo, sizeof(headerInfo));

	//Create root directory (index-node 0)
	N_IndexNode rootDirIndexNode;
	rootDirIndexNode.accessMode = NOISE_FILE_ACCESS_MODE_OWNER_RW ;//Read/Write
	rootDirIndexNode.address = 0;//USER FILE ADDRESS SPACE
	rootDirIndexNode.size = 8192;
	rootDirIndexNode.ownerUserID = NOISE_FILE_OWNER_ROOT;
	outFile.write((char*)&rootDirIndexNode, sizeof(rootDirIndexNode));

	//i-node table (except the Root i-node 0) and other part can be initialized as 0
	std::vector<char> emptyBuffer(headerInfo.diskCapacity - sizeof(headerInfo) - sizeof(N_IndexNode), 0);
	outFile.write((char*)&emptyBuffer[0], emptyBuffer.size());

	outFile.close();

	return true;
}

bool IFileSystem::InstallVirtualDisk(NFilePath virtualDiskImagePath)
{
	m_pVirtualDisk = new std::fstream(virtualDiskImagePath,std::ios::binary);
	if (m_pVirtualDisk == nullptr || m_pVirtualDisk->is_open() == false)
	{
		DEBUG_MSG("Install Virtual Disk failure: VDisk open failed !");
		return false;
	}

	//init the header
	N_VirtualDiskHeaderInfo headerInfo;
	m_pVirtualDisk->read((char*)&headerInfo, sizeof(headerInfo));

	//magic number
	if (headerInfo.c_magicNumber != c_FileSystemMagicNumber)
	{
		DEBUG_MSG("Install Virtual Disk failure: corrupted Virtual disk image!");
		return false;
	}

	//version check (the version of image and file system should match)
	if (headerInfo.c_versionNumber != c_FileSystemVersion)
	{
		DEBUG_MSG("Install Virtual Disk failure: Version not match!");
		return false;
	}

	//the real capacity of disk (including all content)
	mVDiskCapacity =  headerInfo.diskCapacity;

	//offset that need to be added to USER-SPACE-ADDRESS when using fstream
	//(header and i-node table are skipped)
	mVDiskUserFileOffset = headerInfo.diskHeaderLength;


	//init the i-node table
	uint32_t inodeCount = headerInfo.indexNodeCount;
	m_pIndexNodeList = new std::vector<N_IndexNode>(inodeCount);
	m_pVirtualDisk->read((char*)&m_pIndexNodeList->at(0), inodeCount * sizeof(N_IndexNode));

	//init i-node of current working dir with root
	mCurrentDirIndexNode = m_pIndexNodeList->at(0);

	//init the ALLOCATOR of ¡¾I-NODE¡¿ and  ¡¾Free User Space¡¿
	m_pIndexNodeAllocator = new CAllocator(inodeCount);
	m_pFileAddressAllocator = new CAllocator(mVDiskCapacity - mVDiskUserFileOffset);
	for (uint32_t i = 0; i < m_pIndexNodeList->size(); ++i)
	{
		N_IndexNode& inode =m_pIndexNodeList->at(i);
		m_pIndexNodeAllocator->Allocate(i, 1);
		m_pFileAddressAllocator->Allocate(inode.address, inode.size);
	}
	
	//remaining free space
	mVDiskFreeSpace = m_pFileAddressAllocator->GetFreeSpace();

	mIsVDiskInitialized = true;
	return true;
}

bool IFileSystem::UninstallVirtualDisk()
{
	//close all opened files(in case the user forget to close) (and force them to update data to hard disk)
	uint32_t openedFileCount =  IFactory<IFile>::GetObjectCount();
	for (uint32_t i = 0; i < openedFileCount; ++i)
	{	
		IFileSystem::CloseFile(IFactory<IFile>::GetObjectPtr(i));//forced hard disk write
	}
	IFactory<IFile>::DestroyAllObject();


	//update i-node table (to HARD DISK)
	m_pVirtualDisk->seekp(sizeof(N_VirtualDiskHeaderInfo));
	m_pVirtualDisk->write((char*)&m_pIndexNodeList->at(0), sizeof(N_IndexNode)*m_pIndexNodeList->size());

	m_pVirtualDisk->close();
	delete m_pVirtualDisk;
	m_pVirtualDisk = nullptr;

	mIsVDiskInitialized = false;
	return true;
}

bool IFileSystem::Login(std::string userName, std::string password)
{
	//preset account info are defined here (ACCOUNT system should be independent from FILE system)
	typedef  std::pair<std::string, std::string> LoginInfo;
	static const int presetAccountCount = 3;

	//preset account info
	LoginInfo presetAccounts[presetAccountCount]; 
	presetAccounts[NOISE_FILE_OWNER_NULL] = {"",""};
	presetAccounts[NOISE_FILE_OWNER_ROOT] = { "ROOT","ROOT666666" };
	presetAccounts[NOISE_FILE_OWNER_GUEST] = { "GUEST","GUEST666666" };


	for (int i = 1; i < presetAccountCount; ++i)
	{
		if (userName == presetAccounts[i].first && password == presetAccounts[i].second)
		{
			mLoggedInAccountID = i;//save the logged in user ID for access protection
			DEBUG_MSG("Login Succeeded! User:" + presetAccounts[i].first);
			return true;
		}
	}

	DEBUG_MSG("Login Failed! Illegal account information!");
	return false;
}

uint32_t IFileSystem::GetVDiskCapacity()
{
	return mVDiskCapacity;
}

uint32_t IFileSystem::GetVDiskUsedSize()
{
	return mVDiskCapacity-mVDiskFreeSpace-mVDiskUserFileOffset;
}

uint32_t IFileSystem::GetVDiskFreeSize()
{
	return mVDiskFreeSpace;
}
