
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
	m_pVirtualDiskFile(nullptr),
	m_pFileAddressAllocator(nullptr),
	m_pIndexNodeAllocator(nullptr),
	m_pVirtualDiskImage(nullptr),
	m_pIndexNodeList(nullptr),
	mIsVDiskInitialized(false),
	mLoggedInAccountID(0xffff),
	mVDiskImageSize(0),
	mVDiskCapacity(0),
	mVDiskHeaderLength(0)
{
}

IFileSystem::~IFileSystem()
{
	if (mIsVDiskInitialized)UninstallVirtualDisk();
	delete m_pFileAddressAllocator;
	delete m_pIndexNodeAllocator;
	delete m_pIndexNodeList;
	delete m_pVirtualDiskImage;
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
	//std::vector<char> emptyBuffer(headerInfo.diskCapacity - sizeof(headerInfo) - sizeof(N_IndexNode), 0);
	std::vector<char> emptyBuffer(headerInfo.diskCapacity);
	outFile.write((char*)&emptyBuffer[0], emptyBuffer.size());

	outFile.close();

	return true;
}

bool IFileSystem::InstallVirtualDisk(NFilePath virtualDiskImagePath)
{
	m_pVirtualDiskFile = new std::fstream(virtualDiskImagePath,std::ios::binary);
	if (m_pVirtualDiskFile == nullptr || m_pVirtualDiskFile->is_open() == false)
	{
		DEBUG_MSG("Install Virtual Disk failure: VDisk open failed !");
		return false;
	}

	//load the whole file into memory
	m_pVirtualDiskFile->seekg(0, std::ios::end);
	uint32_t fileSize = m_pVirtualDiskFile->tellg();
	m_pVirtualDiskFile->seekg(0);
	m_pVirtualDiskImage->resize(fileSize);
	m_pVirtualDiskFile->read((char*)&m_pVirtualDiskImage->at(0), fileSize);

	//init the header
	N_VirtualDiskHeaderInfo headerInfo;
	mFunction_ReadData(0, headerInfo);

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
	mVDiskHeaderLength = headerInfo.diskHeaderLength;

	if (fileSize != mVDiskHeaderLength + mVDiskCapacity)
	{
		//simple error check about the data size
		DEBUG_MSG("Install Virtual Disk failure: corrupted Virtual disk image!");
		return false;
	}

	//init the i-node table
	uint32_t inodeCount = headerInfo.indexNodeCount;
	m_pIndexNodeList = new std::vector<N_IndexNode>(inodeCount);
	for (uint32_t i = 0; i < inodeCount; ++i)
	{
		mFunction_ReadData(sizeof(N_VirtualDiskHeaderInfo) + i * sizeof(N_IndexNode), m_pIndexNodeList->at(i));
	}

	//offset that skips header & i-node table
	mVDiskHeaderLength = sizeof(N_VirtualDiskHeaderInfo) + inodeCount * sizeof(N_IndexNode);


	//init i-node of current working dir with root
	m_pCurrentDirIndexNode = &m_pIndexNodeList->at(0);

	//init the ALLOCATOR of ¡¾I-NODE¡¿ and  ¡¾Free User Space¡¿
	m_pIndexNodeAllocator = new CAllocator(inodeCount);
	m_pFileAddressAllocator = new CAllocator(mVDiskCapacity);
	for (uint32_t i = 0; i < m_pIndexNodeList->size(); ++i)
	{
		N_IndexNode& inode =m_pIndexNodeList->at(i);
		m_pIndexNodeAllocator->Allocate(i, 1);
		m_pFileAddressAllocator->Allocate(inode.address, inode.size);
	}

	mIsVDiskInitialized = true;
	return true;
}

bool IFileSystem::UninstallVirtualDisk()
{
	//close all opened files(in case the user forget to close)
	uint32_t openedFileCount =  IFactory<IFile>::GetObjectCount();
	for (uint32_t i = 0; i < openedFileCount; ++i)
	{	
		IFileSystem::CloseFile(IFactory<IFile>::GetObjectPtr(i));//forced hard disk write
	}
	IFactory<IFile>::DestroyAllObject();

	//update i-node table
	uint32_t inodeCount = m_pIndexNodeList->size();
	for (uint32_t i = 0; i < inodeCount; ++i)
	{
		mFunction_WriteData(sizeof(N_VirtualDiskHeaderInfo) + i * sizeof(N_IndexNode), m_pIndexNodeList->at(i));
	}

	//write the image of VD to hard disk
	m_pVirtualDiskFile->seekp(0);
	m_pVirtualDiskFile->write((char*)&m_pVirtualDiskImage->at(0), m_pVirtualDiskImage->size());

	m_pVirtualDiskFile->close();
	m_pVirtualDiskImage->clear();
	delete m_pVirtualDiskFile;
	m_pVirtualDiskFile = nullptr;

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

bool IFileSystem::SetWorkingDir(std::string dir)
{
	//decide if a character is delimiter of a path
	auto isDelim = [](char c) ->bool{return c == '\\' || c == '/'; };

	//analyze directory folders hierarchies
	std::vector<std::string> intermediateFolders;
	if (dir.size() == 0) { DEBUG_MSG("SetWorkingDir failure: empty argument."); return false; }
	if(!isDelim(dir.at(0))){ DEBUG_MSG("SetWorkingDir failure: path must start with \\ or /"); return false; }

	for (int i = 1; i < dir.size();++i)
	{
		if (isDelim(dir.at(i)))
		{
			intermediateFolders.push_back("");
		}
		else
		{
			intermediateFolders.back().push_back(dir.at(i));
		}
	}

	//delimiter could occur at the back of a string, eliminate it.
	if (intermediateFolders.back() == "")intermediateFolders.pop_back();

	//iterate from root i-node
	m_pCurrentDirIndexNode = &m_pIndexNodeList->at(0);

	for (int i = 0; i < intermediateFolders.size(); ++i)
	{
		asdasdasdasd;
	}

	return true;
}

bool IFileSystem::CreateFolder(std::string folderName)
{
	if (folderName.size() > 120)
	{
		DEBUG_MSG("FileSystem :Create folder failed. folder name too long (>120)");
		return false;
	}

	if (folderName.find('\\', 0) == std::string::npos || folderName.find('/', 0) == std::string::npos)
	{
		DEBUG_MSG("FileSystem :Create folder failed. character '\\' and '/' are not permitted.");
		return false;
	}
	
	//read dir info about
	uint32_t dirFileOffset = mVDiskHeaderLength + m_pCurrentDirIndexNode->address;
	uint32_t folderCount = 0,fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT(folderCount);//i-node number list
	std::vector<N_DirFileRecord> subFilesINT(fileCount);//i-node number list

	mFunction_ReadData(dirFileOffset+0, folderCount);
	mFunction_ReadData(dirFileOffset + 4, fileCount);

	for (uint32_t i = 0; i < folderCount; ++i)
		mFunction_ReadData(dirFileOffset + 8 + i * sizeof(N_DirFileRecord), subFolderINT.at(i));

	for (uint32_t i = 0; i < fileCount; ++i)
		mFunction_ReadData(dirFileOffset + 8 +(i+folderCount)  * sizeof(N_DirFileRecord), subFilesINT.at(i));


	//resize of current directory file
	++folderCount;
	m_pFileAddressAllocator->Release(m_pCurrentDirIndexNode->address, m_pCurrentDirIndexNode->size);
	m_pCurrentDirIndexNode->size = 8 + (folderCount + fileCount) * sizeof(N_DirFileRecord);
	uint32_t newAddress = m_pFileAddressAllocator->Allocate(m_pCurrentDirIndexNode->size);
	m_pCurrentDirIndexNode->address = newAddress;

	//Create dir-file for child folder :
	//---1, create i-node
	uint32_t childDirFileINodeNumber = 0;
	asdasdasd;
	//---2, allocate space
	asdasdasd;
	//---3, init data
	asdasdasd;



	//obtain new i-node number , then update resized current dir file
	subFolderINT.push_back(N_DirFileRecord(folderName, childDirFileINodeNumber));

	mFunction_WriteData(dirFileOffset + 0, folderCount);
	mFunction_WriteData(dirFileOffset + 4, fileCount);

	for (uint32_t i = 0; i < folderCount; ++i)
		mFunction_WriteData(dirFileOffset + 8 + i * sizeof(N_DirFileRecord), subFolderINT.at(i));

	for (uint32_t i = 0; i < fileCount; ++i)
		mFunction_WriteData(dirFileOffset + 8 + (i + folderCount) * sizeof(N_DirFileRecord), subFilesINT.at(i));



	return true;
}

uint32_t IFileSystem::GetVDiskCapacity()
{
	return mVDiskCapacity;
}

uint32_t IFileSystem::GetVDiskUsedSize()
{
	return mVDiskCapacity-m_pFileAddressAllocator->GetFreeSpace();
}

uint32_t IFileSystem::GetVDiskFreeSize()
{
	return m_pFileAddressAllocator->GetFreeSpace();
}

const uint32_t IFileSystem::GetNameMaxLength()
{
	return c_FileAndDirNameMaxLength;
}


/**********************************************

							PRIVATE

************************************************/

template<typename T>
inline void IFileSystem::mFunction_ReadData(uint32_t srcOffset, T & destData)
{
	memcpy_s(&destData,sizeof(T), &m_pVirtualDiskImage->at(srcOffset),sizeof(T));
}

template<typename T>
inline void IFileSystem::mFunction_WriteData(uint32_t destOffset, T& srcData)
{
	memcpy_s(&m_pVirtualDiskImage->at(srcOffset), sizeof(T), &srcData, sizeof(T));
}