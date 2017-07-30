#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <unordered_map>

typedef std::string N_UID;
typedef  std::string NFilePath;
typedef unsigned int UINT;
typedef int  BOOL;

extern std::ofstream* g_pLogFile;
#define DEBUG_MSG(msg) *g_pLogFile<<msg<<std::endl;
#define ERROR_MSG(msg) *g_pLogFile<<msg<<std::endl;
#define TRUE 1
#define FALSE 0


#include "IFactory.h"
#include "Allocator.h"
#include "FileSystem.h"
