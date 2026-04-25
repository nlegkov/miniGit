#ifndef HISTORY_H
#define HISTORY_H

#include "common.h"

void diff_git(const char* commit_name1, const char* commit_name2);
int read_lines_from_file(FILE* f, char*** out_lines, int* out_count);
void ldc(char* path1, char* path2);
void commit(const char* message);
void log_git(const char* commit_name, int num_commit_log);
void log_git_for(const char* commit_1, const char* commit_2);

#endif