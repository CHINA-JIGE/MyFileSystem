
/***********************************************************************

									cpp£ºFile System

************************************************************************/

#include "Noise3D.h"

using namespace Noise3D::Core;

/***************************************************
								IFILE
****************************************************/

IFile::IFile() :
	mIsFileOpened(false),
	mFileIndexNodeNumber(0xffffffff),
	mFileSize(0),
	m_pFileBuffer(nullptr)
{

}

IFile::~IFile()
{
}

UINT IFile::GetFileSize()
{
	return mFileSize;
}

void IFile::Read(char* pOutData, uint32_t startIndex, uint32_t size)
{
	if (!mIsFileOpened)return;

	if (startIndex + size <= mFileSize)
	{
		uint32_t remainSize = size - startIndex;
		//copy 
		memcpy_s(pOutData, remainSize, m_pFileBuffer + startIndex, remainSize);
	}
	else
	{
		ERROR_MSG("IFile : 'Read' failure! Index out of boundary!");
	}
}

void IFile::Write(char * pSrcData, uint32_t startIndex, uint32_t size)
{
	if (!mIsFileOpened)return;

	if (startIndex + size <= mFileSize)
	{
		uint32_t remainSize = size - startIndex;
		//copy 
		memcpy_s(m_pFileBuffer+startIndex, remainSize, pSrcData, remainSize);
	}
	else
	{
		ERROR_MSG("IFile : 'Write' failure! Index out of boundary!");
	}
}



/**************************************************************

									FILE SYSTEM

***************************************************************/

IFileSystem::IFileSystem() :
	IFactory<IFile>(131072),
	m_pVirtualDiskFile(nullptr),
	m_pFileAddressAllocator(nullptr),
	m_pIndexNodeAllocator(nullptr),
	m_pVirtualDiskImage(nullptr),
	m_pIndexNodeList(nullptr),
	m_pCurrentDirIndexNode(nullptr),
	m_pCurrentWorkingDir(new std::string("")),
	mIsVDiskInitialized(false),
	mLoggedInAccountID(0xff),
	mVDiskImageSize(0),
	mVDiskCapacity(0),
	mVDiskHeaderLength(0)
{
}

IFileSystem::~IFileSystem()
{
#define deletePtr(ptr) if(ptr!=nullptr)delete ptr;
	if (mIsVDiskInitialized)UninstallVirtualDisk();
	deletePtr(m_pFileAddressAllocator);
	deletePtr(m_pIndexNodeAllocator);
	deletePtr(m_pIndexNodeList);
	deletePtr(m_pCurrentWorkingDir);
	deletePtr(m_pVirtualDiskImage);
}

bool IFileSystem::CreateVirtualDisk(NFilePath filePath, NOISE_VIRTUAL_DISK_CAPACITY cap)
{
	std::ofstream outFile(filePath.c_str(),std::ios::binary);
	if (!outFile.is_open())
	{
		ERROR_MSG("FileSystem: Create virtual disk failed! file cannot be created.");
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
	rootDirIndexNode.size =8;
	rootDirIndexNode.ownerUserID = NOISE_FILE_OWNER_ROOT;
	outFile.write((char*)&rootDirIndexNode, sizeof(rootDirIndexNode));

	//i-node table (except the Root i-node 0) and other part can be initialized as 0
	//std::vector<char> emptyBuffer(headerInfo.diskCapacity - sizeof(headerInfo) - sizeof(N_IndexNode), 0);
	//(2017.7.27)capacity only indicates file space, not including index node table
	std::vector<char> emptyBuffer((headerInfo.indexNodeCount-1) *sizeof(N_IndexNode) +  headerInfo.diskCapacity);
	outFile.write((char*)&emptyBuffer[0], emptyBuffer.size());

	outFile.close();

	return true;
}

bool IFileSystem::InstallVirtualDisk(NFilePath virtualDiskImagePath)
{
	if (mIsVDiskInitialized)
	{
		ERROR_MSG("Install Virtual Disk failure: virtual disk is already installed !!");
		return false;
	}

	m_pVirtualDiskFile = new std::fstream(virtualDiskImagePath, std::ios::binary | std::ios::in | std::ios::out);
	if (m_pVirtualDiskFile == nullptr || m_pVirtualDiskFile->is_open() == false)
	{
		ERROR_MSG("Install Virtual Disk failure: virtual disk image open failed !");
		return false;
	}

	//load the whole file into memory
	m_pVirtualDiskFile->seekg(0, std::ios::end);
	uint32_t fileSize = m_pVirtualDiskFile->tellg();
	if (fileSize<20)
	{
		ERROR_MSG("Install Virtual Disk failure: corrupted Virtual disk image!");
		return false;
	}


	m_pVirtualDiskFile->seekg(0);
	m_pVirtualDiskImage= new std::vector<char>(fileSize);
	m_pVirtualDiskFile->read((char*)&m_pVirtualDiskImage->at(0), fileSize);

	//init the header
	N_VirtualDiskHeaderInfo headerInfo;
	mFunction_ReadData(0, headerInfo);

	//magic number
	if (headerInfo.c_magicNumber != c_FileSystemMagicNumber)
	{
		ERROR_MSG("Install Virtual Disk failure: corrupted Virtual disk image!");
		return false;
	}

	//version check (the version of image and file system should match)
	if (headerInfo.c_versionNumber != c_FileSystemVersion)
	{
		ERROR_MSG("Install Virtual Disk failure: Version not match!");
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
		ERROR_MSG("Install Virtual Disk failure: corrupted Virtual disk image!");
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
	*m_pCurrentWorkingDir = "\\";

	//init the ALLOCATOR of ¡¾I-NODE¡¿ and  ¡¾Free User Space¡¿
	m_pIndexNodeAllocator = new CAllocator(inodeCount);
	m_pFileAddressAllocator = new CAllocator(mVDiskCapacity);
	for (uint32_t i = 0; i < m_pIndexNodeList->size(); ++i)
	{
		N_IndexNode& inode =m_pIndexNodeList->at(i);
		if(inode.ownerUserID!=NOISE_FILE_OWNER_NULL)m_pIndexNodeAllocator->Allocate(i, 1);
		m_pFileAddressAllocator->Allocate(inode.address, inode.size);
	}


	mIsVDiskInitialized = true;
	return true;
}

void IFileSystem::UninstallVirtualDisk()
{
	if (!mIsVDiskInitialized)
	{
		ERROR_MSG("Install Virtual Disk failure: virtual disk was not installed !!");
		return;
	}

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

	m_pFileAddressAllocator->ReleaseAllSpace();
	m_pIndexNodeAllocator->ReleaseAllSpace();

	mIsVDiskInitialized = false;
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

	ERROR_MSG("Login Failed! Illegal account information!");
	return false;
}

bool IFileSystem::SetWorkingDir(std::string dir)
{
	//decide if a character is delimiter of a path
	auto isDelim = [](char c) ->bool{return c == '\\' || c == '/'; };

	//analyze directory folders hierarchies
	std::vector<std::string> intermediateFolders;
	if (dir.size() == 0) { ERROR_MSG("SetWorkingDir failure: empty argument."); return false; }
	if(!isDelim(dir.at(0))){ ERROR_MSG("SetWorkingDir failure: path must start with \\ or /"); return false; }

	//split path into folder name with delimiter
	for (uint32_t i = 1; i < dir.size();++i)
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
	N_IndexNode*		pOriginIndexNode = m_pCurrentDirIndexNode;//restore when failure occur
	m_pCurrentDirIndexNode = &m_pIndexNodeList->at(0);

	for (auto& folderName: intermediateFolders)
	{

		uint32_t folderCount = 0, fileCount = 0;
		std::vector<N_DirFileRecord> subFolderINT;//i-node number list
		std::vector<N_DirFileRecord> subFilesINT;//i-node number list

		mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

		for (auto& existingFolder: subFolderINT)
		{
			//match existing child folder
			if (folderName == existingFolder.name)
			{
				m_pCurrentDirIndexNode = &m_pIndexNodeList->at(existingFolder.indexNodeId);
				break;
			};
		}

		//loop-ed through folder names of current level, no match
		m_pCurrentDirIndexNode = pOriginIndexNode;//restore former i-node
		ERROR_MSG("SetWorkingDirectory: No such directory .");
		return false;
	}

	//SUCCEED
	return true;
}

std::string IFileSystem::GetWorkingDir()
{
	return *m_pCurrentWorkingDir;
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
		DEBUG_MSG("FileSystem :Create folder failed. character '\\' and '/' are not permitted in a NAME .");
		return false;
	}
	
	if (m_pFileAddressAllocator->GetFreeSpace() < sizeof(N_DirFileRecord))
	{
		DEBUG_MSG("FileSystem :Create folder failed. the entire address space has been occupied.");
		return false;
	}
		
	if (m_pIndexNodeAllocator->IsAddressSpaceRanOut())
	{
		DEBUG_MSG("FileSystem :Create folder failed. No available index-node left for allocation.");
		return false;
	}


	//Create dir-file for child folder :
	//---1, create i-node
	//---2, allocate space
	//---3, init data
	uint32_t childDirFileAddr = m_pFileAddressAllocator->Allocate(2*sizeof(uint32_t));
	uint32_t childDirFileINodeNum = m_pIndexNodeAllocator->Allocate(1);

	N_IndexNode inode = m_pIndexNodeList->at(childDirFileINodeNum);
	inode.accessMode = NOISE_FILE_ACCESS_MODE_OWNER_RW;
	inode.address = childDirFileAddr;
	inode.size = 2 * sizeof(uint32_t);//refer to the doc
	inode.ownerUserID = NOISE_FILE_OWNER_ROOT;
	m_pIndexNodeList->at(childDirFileINodeNum) = inode;//assign value to allocated i-node

	uint32_t zeroCount = 0;
	mFunction_WriteData(mVDiskHeaderLength + childDirFileAddr + 0, zeroCount);//folder count
	mFunction_WriteData(mVDiskHeaderLength + childDirFileAddr + 4, zeroCount);//file count


#pragma region MODIFY DIR FILE FOR CREATE CHILD FILES

	//read dir info about
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	//resize of current directory file
	++folderCount;
	m_pFileAddressAllocator->Release(m_pCurrentDirIndexNode->address, m_pCurrentDirIndexNode->size);
	uint32_t newSize = 8 + (folderCount + fileCount) * sizeof(N_DirFileRecord);
	uint32_t newAddress = m_pFileAddressAllocator->Allocate(newSize);
	m_pCurrentDirIndexNode->address = newAddress;
	m_pCurrentDirIndexNode->size = newSize;

	//OBTAIN new i-node number !!! UPDATE resized current dir file
	subFolderINT.push_back(N_DirFileRecord(folderName, childDirFileINodeNum));
	mFunction_WriteDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

#pragma endregion

	return true;
}

bool IFileSystem::DeleteFolder(std::string folderName)
{
	if (folderName.size() > 120)
	{
		DEBUG_MSG("FileSystem :Delete folder failed. folder name not exist. ");
		return false;
	}

	if (folderName.find('\\', 0) == std::string::npos || folderName.find('/', 0) == std::string::npos)
	{
		DEBUG_MSG("FileSystem :Delete folder failed. Folder name not exist.  Character '\\' and '/' are not permitted in a NAME .");
		return false;
	}

	//--working dir
	//			|-----fileA
	//			|-----fileA
	//			|-----folderA
	//						|--.....
	//			|-----folderB
	//						|--.....
	//			|-----targetFolder
	//						|---folders and files need to be recursively removed

	//read dir info
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	//check if target directory exist 
	bool isFolderFound = false;
	uint32_t targetIndexNodeNum = 0xffffffff;
	for (auto pIter = subFolderINT.begin();pIter!=subFolderINT.end();++pIter)
	{
		//match existing child folder
		if (folderName == pIter->name)
		{
			isFolderFound = true;
			//delete item
			targetIndexNodeNum =	 pIter->indexNodeId;


			//delete all child folders and files under target folder (including this folder itself)
			//(by Releasing i-nodes and address segment)
			//NOTE: if there is an opened file under target folder, then deletion will fail
			if (!mFunction_RecursiveFolderDelete(targetIndexNodeNum))
			{
				//error message will be given within function 'RecursiveFolderDelete'
				return false;
			};

			//resize of CURRENT LEVEL directory file
			--folderCount;
			m_pFileAddressAllocator->Release(m_pCurrentDirIndexNode->address, m_pCurrentDirIndexNode->size);
			uint32_t newAddress = m_pFileAddressAllocator->Allocate(m_pCurrentDirIndexNode->size);
			m_pCurrentDirIndexNode->address = newAddress;
			m_pCurrentDirIndexNode->size = 8 + (folderCount + fileCount) * sizeof(N_DirFileRecord);

			//then update resized dir file
			subFolderINT.erase(pIter);
			mFunction_WriteDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);


			break;
		};
	}

	if (!isFolderFound)
	{
		DEBUG_MSG("FileSystem :Delete folder failed. folder name not exist. ");
		return false;
	}


	return true;
}

void IFileSystem::EnumerateFilesAndDirs(NFileSystemEnumResult & outResult)
{
	//read directory file
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> folderList, fileList;
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, folderList,fileList);

	//output files and dirs
	outResult.folderList.reserve(folderCount);
	outResult.fileList.reserve(fileCount);
	for (auto& folder : folderList)outResult.folderList.push_back(folder.name);
	for (auto& file : fileList)
	{
		N_IndexNode* pNode = &m_pIndexNodeList->at(file.indexNodeId);
		//system files (like directory file are hidden from users)
		if (pNode->ownerUserID != NOISE_FILE_OWNER_ROOT || pNode->ownerUserID != NOISE_FILE_OWNER_NULL)
		{
			outResult.fileList.push_back(*pNode);
		}
	}
}

bool IFileSystem::CreateFile(std::string fileName, uint32_t byteSize, NOISE_FILE_ACCESS_MODE acMode)
{
	//new file space
	uint32_t childFileAddr = m_pFileAddressAllocator->Allocate(byteSize);
	uint32_t childFileINodeNum = m_pIndexNodeAllocator->Allocate(1);
	if (childFileAddr == c_invalid_alloc_address)
	{
		ERROR_MSG("FileSystem :Create File failed.Not Enough space.");
		return false;
	}

	if (childFileINodeNum == c_invalid_alloc_address)
	{
		ERROR_MSG("FileSystem :Create File failed. Not Enough index nodes.");
		return false;
	}

	//new INDEX NODE the file
	N_IndexNode newFileIndexNode;
	newFileIndexNode.accessMode = acMode;
	newFileIndexNode.address = childFileAddr;
	newFileIndexNode.ownerUserID = mLoggedInAccountID;
	newFileIndexNode.size = byteSize;
	m_pIndexNodeList->at(childFileINodeNum) = newFileIndexNode;


#pragma region MODIFY DIR FILE FOR CREATE CHILD FILES

	//read dir info about
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	//resize of current directory file
	++fileCount;
	m_pFileAddressAllocator->Release(m_pCurrentDirIndexNode->address, m_pCurrentDirIndexNode->size);
	uint32_t newSize = 8 + (folderCount + fileCount) * sizeof(N_DirFileRecord);
	uint32_t newAddress = m_pFileAddressAllocator->Allocate(newSize);
	m_pCurrentDirIndexNode->address = newAddress;
	m_pCurrentDirIndexNode->size = newSize;

	//OBTAIN new i-node number !!! UPDATE resized wroking dir's  dir  file
	subFilesINT.push_back(N_DirFileRecord(fileName, childFileINodeNum));
	mFunction_WriteDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

#pragma endregion


	return true;
}

bool IFileSystem::DeleteFile(std::string fileName)
{
	if (fileName.size() > 120)
	{
		DEBUG_MSG("FileSystem :Delete file failed. file not exist. (file name too long)");
		return false;
	}

	if (fileName.find('\\', 0) == std::string::npos || fileName.find('/', 0) == std::string::npos)
	{
		DEBUG_MSG("FileSystem :Delete file failed. file not exist. Character '\\' and '/' are not permitted in a NAME .");
		return false;
	}

	//read dir info about
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	//try to find target file
	bool isFileFound = false;
	for (auto pIter = subFolderINT.begin(); pIter != subFolderINT.end(); ++pIter)
	{
		//match existing child folder
		if (fileName == pIter->name)
		{
			//delete item
			uint32_t targetIndexNodeNum = pIter->indexNodeId;
			N_IndexNode* pINode = &m_pIndexNodeList->at(targetIndexNodeNum);
			if (pINode->isFileOpened)
			{
				DEBUG_MSG("FileSystem :Delete file failed. file is OPEN-ED.");
				return false;
			}

			mFunction_ReleaseFileSpace(targetIndexNodeNum);
			subFolderINT.erase(pIter);
			isFileFound = true;
			break;
		};
	}

	if (!isFileFound)
	{
		DEBUG_MSG("FileSystem :Delete file failed. file not found. ");
		return false;
	}


	//resize of current directory file
	--fileCount;
	m_pFileAddressAllocator->Release(m_pCurrentDirIndexNode->address, m_pCurrentDirIndexNode->size);
	uint32_t newSize = 8 + (folderCount + fileCount) * sizeof(N_DirFileRecord);
	uint32_t newAddress = m_pFileAddressAllocator->Allocate(newSize);
	m_pCurrentDirIndexNode->address = newAddress;
	m_pCurrentDirIndexNode->size = newSize;
	mFunction_WriteDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	return true;
}

IFile * IFileSystem::OpenFile(std::string fileName)
{
	if (fileName.size() > 120)
	{
		ERROR_MSG("FileSystem :Delete file failed. file not exist. (file name too long)");
		return false;
	}

	if (fileName.find('\\', 0) == std::string::npos || fileName.find('/', 0) == std::string::npos)
	{
		ERROR_MSG("FileSystem :Delete file failed. file not exist. Character '\\' and '/' are not permitted in a NAME .");
		return false;
	}

	//read dir info about
	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	mFunction_ReadDirectoryFile(m_pCurrentDirIndexNode->address, folderCount, fileCount, subFolderINT, subFilesINT);

	//try to find target file
	for (auto pIter = subFolderINT.begin(); pIter != subFolderINT.end(); ++pIter)
	{
		//match existing child folder
		if (fileName == pIter->name)
		{
			uint32_t targetIndexNodeNum = pIter->indexNodeId;
			N_IndexNode* pINode = &m_pIndexNodeList->at(targetIndexNodeNum);
			if (pINode->isFileOpened)
			{
				ERROR_MSG("FileSystem :Open file failed. file is already OPEN-ED.");
				return false;
			}

			//create new file interface and init
			IFile* pNewFile =IFactory<IFile>::CreateObject(fileName);
			pNewFile->mFileIndexNodeNumber = targetIndexNodeNum;
			pNewFile->m_pFileBuffer = &m_pVirtualDiskImage->at(mVDiskHeaderLength + pINode->address);
			pNewFile->mFileSize = pINode->size;
			pNewFile->mIsFileOpened = true;

			return pNewFile;
		};
	}

	ERROR_MSG("FileSystem : Open file failed. file not found. ");
	return nullptr;
}

bool IFileSystem::CloseFile(IFile * pFile)
{
	pFile->mIsFileOpened = false;

	N_IndexNode* pINode = &m_pIndexNodeList->at(pFile->mFileIndexNodeNumber);
	pINode->isFileOpened = false;

	IFactory<IFile>::DestroyObject(pFile);

	return false;
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
	memcpy_s(&m_pVirtualDiskImage->at(destOffset), sizeof(T), &srcData, sizeof(T));
}

bool IFileSystem::mFunction_NameValidation(const std::string & name)
{
	return false;
}

void IFileSystem::mFunction_ReadDirectoryFile(uint32_t dirFileAddress, uint32_t & outFolderCount, uint32_t & outFileCount, std::vector<N_DirFileRecord>& outChildFolders, std::vector<N_DirFileRecord>& outChildFiles)
{

	mFunction_ReadData(mVDiskHeaderLength + dirFileAddress + 0, outFolderCount);
	mFunction_ReadData(mVDiskHeaderLength + dirFileAddress + 4, outFileCount);

	outChildFolders.resize(outFolderCount);
	outChildFiles.resize(outFileCount);

	for (uint32_t i = 0; i < outFolderCount; ++i)
		mFunction_ReadData(mVDiskHeaderLength + dirFileAddress + 8 + i * sizeof(N_DirFileRecord), outChildFolders.at(i));

	for (uint32_t i = 0; i < outFileCount; ++i)
		mFunction_ReadData(mVDiskHeaderLength + dirFileAddress + 8 + (i + outFolderCount) * sizeof(N_DirFileRecord), outChildFiles.at(i));
}

void IFileSystem::mFunction_WriteDirectoryFile(uint32_t dirFileAddress, uint32_t inFolderCount, uint32_t inFileCount, std::vector<N_DirFileRecord>& inChildFolders, std::vector<N_DirFileRecord>& inChildFiles)
{
	mFunction_WriteData(mVDiskHeaderLength + dirFileAddress + 0, inFolderCount);
	mFunction_WriteData(mVDiskHeaderLength + dirFileAddress + 4, inFileCount);

	for (uint32_t i = 0; i < inFolderCount; ++i)
		mFunction_WriteData(mVDiskHeaderLength + dirFileAddress + 8 + i * sizeof(N_DirFileRecord), inChildFolders.at(i));

	for (uint32_t i = 0; i < inFileCount; ++i)
		mFunction_WriteData(mVDiskHeaderLength + dirFileAddress + 8 + (i + inFolderCount) * sizeof(N_DirFileRecord), inChildFiles.at(i));
}

void IFileSystem::mFunction_ReleaseFileSpace(uint32_t fileIndexNodeNum)
{
	//release file storage and index node
	N_IndexNode* pFileINode = &m_pIndexNodeList->at(fileIndexNodeNum);
	m_pFileAddressAllocator->Release(pFileINode->address, pFileINode->size);
	m_pIndexNodeAllocator->Release(fileIndexNodeNum, 1);
	pFileINode->reset();
}

bool IFileSystem::mFunction_RecursiveFolderDelete(uint32_t dirFileIndexNodeNum)
{
	//desc : delete all child-folders and files under this folder including the folder itself

	uint32_t folderCount = 0, fileCount = 0;
	std::vector<N_DirFileRecord> subFolderINT;//i-node number list
	std::vector<N_DirFileRecord> subFilesINT;//i-node number list
	N_IndexNode* pNode = &m_pIndexNodeList->at(dirFileIndexNodeNum);

	mFunction_ReadDirectoryFile(pNode->address, folderCount, fileCount, subFilesINT, subFilesINT);

	//delete files under current directory
	for (auto& existingChildFiles : subFilesINT)
	{
		uint32_t fileINodeNum = existingChildFiles.indexNodeId;
		N_IndexNode* pINode = &m_pIndexNodeList->at(fileINodeNum);
		//if an opened file is found
		if (pINode->isFileOpened)
		{
			DEBUG_MSG("FileSystem :Delete folder failed. a file under this folder is opened. Deletion procedure terminated.");
			DEBUG_MSG("FileSystem :Opened file name:" << existingChildFiles.name);
			return false;
		}

		mFunction_ReleaseFileSpace(fileINodeNum);
	}

	for (auto& existingChildFolder : subFolderINT)
	{
		uint32_t dirFileINodeNum = existingChildFolder.indexNodeId;

		//recursive  deletion
		mFunction_RecursiveFolderDelete(dirFileINodeNum);
		

		mFunction_ReleaseFileSpace(dirFileINodeNum);
	}
		
	mFunction_ReleaseFileSpace(dirFileIndexNodeNum);

	return true;
}

