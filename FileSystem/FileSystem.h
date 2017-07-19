
/***********************************************************************

									h£ºFile System

			Desc: A File System that manage files on a "Virtual Disk"
			(which is actually a big binary on the disk)

			Author: Jige (this is also served as an OS course project)

************************************************************************/

#pragma once

//login		---	IFileSystem::Login
//dir			---	IFileSystem::EnumerateFilesAndDirs
//create	---	IFileSystem::CreateFile
//delete	---	IFileSystem::DeleteFile
//open		---	IFileSystem::OpenFile
//close		---	IFileSystem::CloseFile
//read		---	IFile::GetFileData
//write		---	IFileSystem::Write


namespace Noise3D
{
	namespace Core
	{
		struct N_IndexNode
		{
			N_IndexNode():ownerUserID(0),accessMode(0),address(0),size(0){}
			uint16_t ownerUserID;//userID of this file user (system file are owned by ROOT : 1 , i-node that is not in use is owned by NULL:0)
			uint16_t accessMode;//flag can be combined by 'OR' operation
			uint32_t address;
			uint32_t size;//file byte size
		};

		//result of enumeration of target directory
		struct NFileSystemEnumResult
		{
			std::vector<N_IndexNode> fileList;
			std::vector<std::string> dirList;
		};

		enum NOISE_FILE_TYPE
		{
			NOISE_FILE_TYPE_DIRECTORY=0,
			NOISE_FILE_TYPE_USER=1,
		};

		enum NOISE_FILE_ACCESS_MODE
		{
			NOISE_FILE_ACCESS_MODE_OWNER_READ=1,
			NOISE_FILE_ACCESS_MODE_OWNER_WRITE=2,
			NOISE_FILE_ACCESS_MODE_OWNER_RW=3,
			NOISE_FILE_ACCESS_MODE_OWNER_EXECUTE=4
		};

		enum NOISE_VIRTUAL_DISK_CAPACITY
		{
			NOISE_VIRTUAL_DISK_CAPACITY_128MB,
			NOISE_VIRTUAL_DISK_CAPACITY_256MB,
			NOISE_VIRTUAL_DISK_CAPACITY_512MB,
			NOISE_VIRTUAL_DISK_CAPACITY_1GB
		};

		enum NOISE_FILE_OWNER
		{
			NOISE_FILE_OWNER_NULL=0,
			NOISE_FILE_OWNER_ROOT=1,
			NOISE_FILE_OWNER_GUEST=2
		};

		//--------------------------------------------
		class IFile;

		class /*_declspec(dllexport)*/ IFileSystem:
			public IFactory<IFile>
		{
		public:

			IFileSystem();

			~IFileSystem();

			//create a virtual disk on hard disk (a binary file)
			bool CreateVirtualDisk(NFilePath filePath, NOISE_VIRTUAL_DISK_CAPACITY cap);

			//A virtual Disk is actually a big binary file 
			bool InstallVirtualDisk(NFilePath virtualDiskImagePath);

			//update File system info to hard disk
			bool UninstallVirtualDisk();


			bool Login(std::string userName, std::string password);

			void SetWorkingDir(std::string dir);//working dir within the virtual disk

			void GetWorkingDir(std::string& dir);

			void EnumerateFilesAndDirs(std::string dir, NFileSystemEnumResult& outResult);

			void EnumerateFilesAndDirsOfWorkingDir(NFileSystemEnumResult& outResult);

			bool CreateFile(std::string filePath, UINT byteSize);//a new file under current working directory

			bool DeleteFile(std::string filePath);//can be done only if the file is CLOSED!!

			IFile* OpenFile(std::string filePath);

			bool CloseFile(IFile* pFile);//SAVE and UPDATE data to hard disk

			uint32_t GetVDiskCapacity();

			uint32_t GetVDiskUsedSize();

			uint32_t GetVDiskFreeSize();//free space left for USER FILE in virtual disk

		private:

			struct N_VirtualDiskHeaderInfo
			{
				const uint32_t c_magicNumber = c_FileSystemMagicNumber;
				const uint32_t c_versionNumber = c_FileSystemVersion;
				uint32_t diskCapacity;
				uint32_t diskHeaderLength;//including i-node table
				uint32_t indexNodeCount;
				//i-node table
			};

			bool				mFunction_CreateFile(std::string filePath, UINT byteSize, NOISE_FILE_TYPE type);

			static const uint32_t	c_FileSystemMagicNumber = 0x12345678;
			static const uint32_t	c_FileSystemVersion = 0x20170716;//init will check file system version
			CAllocator*	m_pIndexNodeAllocator;
			CAllocator*	m_pFileAddressAllocator;
			std::fstream*							m_pVirtualDisk;
			std::vector<N_IndexNode>*	m_pIndexNodeList;//i-node list
			N_IndexNode		mCurrentDirIndexNode;
			bool						mIsVDiskInitialized;
			uint16_t				mLoggedInAccountID;
			uint32_t				mVDiskCapacity;
			uint32_t				mVDiskUserFileOffset;	//(header and i-node table are skipped)
			uint32_t				mVDiskFreeSpace;//computed in initialization
		};


		class IFile
		{
		public:

			UINT	GetFileSize();
			//read
			const char*	GetFileData();
			//write, but not immediately update to hard disk
			void Write(char* pSrcData, uint32_t startIndex, uint32_t size);

		private:

			friend IFactory<IFile>;
			friend IFileSystem;
			IFile();
			~IFile();

			bool mIsFileNeedUpdate;//file has been written, data needs to write to hard disk
			UINT mFileByteSize;
			char* m_pFileBuffer;//initiated by FileSystem

		};


	}
}