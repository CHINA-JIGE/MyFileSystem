#include "Noise3D.h"

using namespace Noise3D::Core;
IFileSystem fs;
std::ofstream* g_pLogFile;

//#define TEST_STAGE_CREATE

#ifdef TEST_STAGE_CREATE
int main()
{
	IFileSystem fs;
	fs.CreateVirtualDisk("666.nvd", NOISE_VIRTUAL_DISK_CAPACITY_128MB);

	return 0;
};

#else

void InfoOfWorkingDir()
{
	N_FileSystemEnumResult enumRes;
	fs.EnumerateFilesAndDirs(enumRes);
	DEBUG_MSG("********************************");
	DEBUG_MSG("Folder & File Enumeration：");
	DEBUG_MSG("<CurrentFolder>:");
	DEBUG_MSG("<Child Folders>:");
	for (auto e : enumRes.folderList)
	{
		DEBUG_MSG(e);
	}
	DEBUG_MSG("<Child Files>:");
	for (auto e : enumRes.fileList)
	{
		DEBUG_MSG(e.name << "\t address:" << e.address << "\t end:" << e.address + e.size << "\t size:" << e.size);
	}
	DEBUG_MSG("");
}

int main()
{
	g_pLogFile = new std::ofstream("log.txt", std::ios::trunc);

	fs.InstallVirtualDisk("666.nvd");

	bool b =fs.SetWorkingDir("/asdasd");//xxx
	b = fs.SetWorkingDir("/asdasd/");//xxx
	b = fs.SetWorkingDir("asdasd");//xxx
	b = fs.SetWorkingDir("/");//

	b = fs.CreateFolder("/213/");//xxx
	b = fs.CreateFolder("/msad/");//xxx
	b = fs.CreateFolder("jige");//
	b = fs.CreateFolder("jige/");
	b = fs.CreateFolder("a1");//
	b = fs.CreateFolder("a1");//xxx
	b = fs.CreateFolder("b2");//
	b = fs.CreateFolder("c3");//

	InfoOfWorkingDir();

	b = fs.CreateFile("646.txt", 600, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.CreateFile("123.aaa", 30, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.CreateFile("asdasdasd.avi", 1200, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.CreateFile("jige666.asd", 1200, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.CreateFile("heiheihei.cpp", 1200, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	InfoOfWorkingDir();

	b = fs.SetWorkingDir("/jige");//
	b = fs.SetWorkingDir("/jige/");//
	b = fs.SetWorkingDir("/a1/");//
	b = fs.SetWorkingDir("a23");//xxx
	b = fs.SetWorkingDir("/");//
	b = fs.DeleteFolder("a1/");//xxx
	b = fs.DeleteFolder("/a1");//xxx
	b = fs.DeleteFolder("a1");//
	InfoOfWorkingDir();
	b = fs.DeleteFolder("a1");//xxx
	InfoOfWorkingDir();
	b = fs.DeleteFolder("a2");//xxx
	InfoOfWorkingDir();
	b = fs.DeleteFolder("b2");//
	InfoOfWorkingDir();
	b = fs.DeleteFile("123.txt");//xxx
	InfoOfWorkingDir();
	b = fs.DeleteFile("123.aaa");//
	InfoOfWorkingDir();
	b = fs.DeleteFile("123.aaa");//xxx
	InfoOfWorkingDir();
	b = fs.DeleteFile("646.txt");//
	InfoOfWorkingDir();

	b = fs.CreateFolder("testLevel1");
	b = fs.SetWorkingDir("/testLevel1");
	b = fs.CreateFolder("testLevel2");
	b = fs.SetWorkingDir("/testLevel1/testLevel2");
	b = fs.CreateFolder("bottomFolder1");
	b = fs.CreateFolder("bottomFolder2");
	b = fs.CreateFolder("bottomFolder3");
	b = fs.CreateFile("file1",100,NOISE_FILE_ACCESS_MODE_OWNER_WRITE);
	b = fs.CreateFile("file2",200, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.CreateFile("file3", 300, NOISE_FILE_ACCESS_MODE_OWNER_RW);
	b = fs.DeleteFile("file3");
	InfoOfWorkingDir();

	g_pLogFile->close();
	delete g_pLogFile;
	//fs.UninstallVirtualDisk();就看内存映像debug算了先别写回去
	return 0;
};

#endif