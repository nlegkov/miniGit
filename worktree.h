#ifndef WORKTREE_H
#define WORKTREE_H

#include "common.h"

int copy_file(const char* src, const char* dst);
int path_type(const char* path);
void ensure_parent_dirs(const char* file_path);
void hash_file(const char* src, char out[17]);

void add(const char* file_name);
void add_path(const char* path);
void remove_git(const char* file_name);
void status(void);
void walk_directory_and_add(const char* path);
void stage_deleted_from_head_under_path(const char* path);
void stage_file_status(const char* file_name, int new_status);
int is_under_path(const char* base, const char* file);
void unstage_file(const char* file_name);
int get_saved_hash_from_commit(char* commit_name, char* file_name1, char* out);

void status_untracked_files(const char* path);
void status_deleted_files(void);

void checkout_git(const char* commit_name, const char* file_name);
void del_all_file_dir(const char* path);
void checkout_all_file_commit(const char* name);
void checkout_target(const char* name);

int has_untracked_files_recursive(const char* path, const char* head_commit);
int working_tree_is_clean(void);

#endif