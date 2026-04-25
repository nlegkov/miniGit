#ifndef COMMON_H
#define COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include "dirent.h"

typedef long long ll;
typedef unsigned long long ull;

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define access _access
#define mkdir(dir, mode) _mkdir(dir)
#define rmdir _rmdir
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#endif