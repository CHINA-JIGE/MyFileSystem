
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
		enum NOISE_FILE_ACCESS_MODE
		{
			NOISE_FILE_ACCESS_MODE_OWNER_READ = 1,
			NOISE_FILE_ACCESS_MODE_OWNER_WRITE = 2,
			NOISE_FILE_ACCESS_MODE_OWNER_RW = 3,
			NOISE_FILE_ACCESS_MODE_OWNER_EXECUTE = 4
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
			NOISE_FILE_OWNER_NULL = 0,
			NOISE_FILE_OWNER_ROOT = 1,
			NOISE_FILE_OWNER_GUEST = 2
		};

		struct N_IndexNode
		{
			N_IndexNode():ownerUserID(0),isFileOpened(0) ,accessMode(0),address(0),size(0){}

			void reset() { ownerUserID = NOISE_FILE_OWNER_NULL; isFileOpened = 0; accessMode = 0; address = 0; size = 0; }

			uint8_t ownerUserID;//userID of this file user (system file are owned by ROOT : 1 , i-node that is not in use is owned by NULL:0)
			uint8_t	isFileOpened;//false=0,true=1
			uint16_t accessMode;//flag can be combined by 'OR' operation
			uint32_t address;
			uint32_t size;//file byte size
		};

		struct N_FileEnumInfo
		{
			std::string name;
			uint8_t ownerUserID;//userID of this file user (system file are owned by ROOT : 1 , i-node that is not in use is owned by NULL:0)
			uint16_t accessMode;//flag can be combined by 'OR' operation
			uint32_t address;
			uint32_t size;//file byte size
		};

		//result of enumeration of target directory
		struct N_FileSystemEnumResult
		{
			std::vector<N_FileEnumInfo> fileList;
			std::vector<std::string> folderList;
		};


		class IFile;

		//********************************************************************
		//********************************************************************


		class /*_declspec(dllexport)*/ IFileSystem:
			public IFactory<IFile>
		{
		public:

			IFileSystem();

			~IFileSystem();

			//create a virtual disk on hard disk (a binary file)
			bool CreateVirtualDisk(NFilePath filePath, NOISE_VIRTUAL_DISK_CAPACITY cap);

			//load the whole virtual disk IMAGE into memory
			bool InstallVirtualDisk(NFilePath virtualDiskImagePath);

			//write the VDisk image back to hard disk
			void UninstallVirtualDisk();


			bool Login(std::string userName, std::string password);

			bool SetWorkingDir(std::string dir);//working dir within the virtual disk

			std::string GetWorkingDir();

			bool CreateFolder(std::string folderName);

			bool DeleteFolder(std::string folderName);

			void EnumerateFilesAndDirs(N_FileSystemEnumResult& outResult);

			bool CreateFile(std::string fileName, uint32_t byteSize,NOISE_FILE_ACCESS_MODE acMode);//a new file under current working directory

			bool DeleteFile(std::string fileName);//can be done only if the file is CLOSED!!

			IFile* OpenFile(std::string fileName);

			bool CloseFile(IFile* pFile);//SAVE and UPDATE data to hard disk

			uint32_t GetVDiskCapacity();

			uint32_t GetVDiskUsedSize();

			uint32_t GetVDiskFreeSize();//free space left for USER FILE in virtual disk

			const uint32_t GetNameMaxLength();

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

			//items in an directory file
			struct N_DirFileRecord
			{
				N_DirFileRecord() { for (int i = 0; i < 124; ++i)name[i] = 0; indexNodeId = 0; }
				N_DirFileRecord(std::string _name, uint32_t iNode) { for (int i = 0; i < 124; ++i)name[i] = 0; for (int i = 0; i < int(_name.size()); ++i)name[i] = _name.at(i); indexNodeId = iNode; }
				char name[124];
				uint32_t indexNodeId;
			};

			template<typename T>
			void				mFunction_ReadData(uint32_t srcOffset,T& destData);//read data from VDisk image

			template<typename T>
			void				mFunction_WriteData(uint32_t destOffset, T& srcData);//write data to VDisk image

			bool				mFunction_NameValidation(const std::string& name);

			void				mFunction_ReadDirectoryFile(uint32_t dirFileAddress,uint32_t& outFolderCount, uint32_t& outFileCount, std::vector<N_DirFileRecord>& outChildFolders, std::vector<N_DirFileRecord>& outChildFiles);

			void				mFunction_WriteDirectoryFile(uint32_t dirFileAddress, uint32_t inFolderCount, uint32_t inFileCount, std::vector<N_DirFileRecord>& inChildFolders, std::vector<N_DirFileRecord>& inChildFiles);

			void				mFunction_ReleaseFileSpace(uint32_t fileIndexNodeNum);

			bool				mFunction_RecursiveFolderDelete(uint32_t dirFileIndexNodeNum);//delete all child-folders and files under this folder including the folder itself

			static const uint32_t	c_FileAndDirNameMaxLength = 120;//sizeof(dirFileItem)-sizeof(i-node)=128-4=124, but for safety, round it to 120
			static const uint32_t	c_FileSystemMagicNumber = 0x12345678;
			static const uint32_t	c_FileSystemVersion = 0x20170727;//init stage check file system version
			static const uint32_t	c_DirectoryFileItemSize = 128;//124+4
			std::fstream*							m_pVirtualDiskFile;
			std::vector<char>*					m_pVirtualDiskImage;//Mapped-virtual disk in lying in memory 
			std::vector<N_IndexNode>*	m_pIndexNodeList;//i-node list
			uint32_t				mVDiskImageSize;//the total size of VDisk
			uint32_t				mVDiskCapacity;//file space capacity
			uint32_t				mVDiskHeaderLength;	//(header and i-node table are skipped)
			CAllocator*			m_pIndexNodeAllocator;
			CAllocator*			m_pFileAddressAllocator;
			bool						mIsVDiskInitialized;
			uint8_t					mLoggedInAccountID;

			N_IndexNode*		m_pCurrentDirIndexNode;
			std::string*			m_pCurrentWorkingDir;
		};


		class IFile
		{
		public:

			UINT	GetFileSize();
			//read
			void Read(char* pOutData,uint32_t startIndex,uint32_t size);
			//write, but not immediately update to hard disk
			void Write(char* pSrcData, uint32_t startIndex, uint32_t size);

		private:

			IFile();
			~IFile();
			friend		IFactory<IFile>;
			friend		IFileSystem;

			bool			mIsFileOpened;//file has been written, data needs to write to hard disk
			bool			mAccessMode_Read;
			bool			mAccessMode_Write;
			uint32_t	mFileIndexNodeNumber;
			uint32_t	mFileSize;
			char*		m_pFileBuffer;
		};
	}
}