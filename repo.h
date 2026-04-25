#ifndef REPO_H
#define REPO_H

#include "common.h"

int get_head_commit(char out_commit[64]);
int get_head_branch_ref(char out_ref[256]);
int get_parent_commit(const char* commit_name, char out_parent[64]);
int resolve_commit_or_branch(const char* name, char out_commit[64], char out_branch[256], int* is_branch);
int is_valid_commit_name(const char* s);
int find_file_commit(const char* commit_name, const char* file_name, char out_commit[20]);
int get_file_path_from_commit(const char* commit_name, const char* file_name, char out_path[256]);
void trim_newline(char* s);
void init(void);
void branch_git(const char* branch_name);

#endif