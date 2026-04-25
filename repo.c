#include "repo.h"

//поиск файла из одного коммита в другом
int find_file_commit(const char* commit_name, const char* file_name, char out_commit[20]) {
    char path[256];
    snprintf(path, sizeof(path), ".mygit/commits/%s/file.txt", commit_name);

    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return 0;
    }

    char word[256];
    while (fscanf(f, "%255s", word) == 1) {
        char tmp[256];
        strcpy(tmp, word);

        char* p = strchr(tmp, '|');
        if (p == NULL) {
            continue;
        }

        *p = '\0';

        if (strcmp(tmp, file_name) == 0) {
            strcpy(out_commit, p + 1);
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

//ищит место расположение файла по заданному коммиту и названию файла
int get_file_path_from_commit(const char* commit_name, const char* file_name, char out_path[256]) {
    char real_commit[20];

    if (!find_file_commit(commit_name, file_name, real_commit)) {
        return 0;
    }

    snprintf(out_path, 256, ".mygit/commits/%s/files/%s", real_commit, file_name);
    return 1;
}

//проверяет название коммита
int is_valid_commit_name(const char* s) {
    if (s == NULL || s[0] != 'c' || s[1] == '\0') {
        return 0;
    }

    for (int i = 1; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
    }

    return 1;
}

void trim_newline(char* s) {
    s[strcspn(s, "\r\n")] = '\0';
}

//функция возвращает какой коммит был последним, тоесть на какой коммит сейчас указывает актуальная ветка
int get_head_commit(char out_commit[64]) {
    FILE* f = fopen(".mygit/HEAD.txt", "r");

    char head_value[256];
    fgets(head_value, sizeof(head_value), f);
    fclose(f);

    trim_newline(head_value);

    if (strncmp(head_value, "ref: ", 5) == 0) {
        char ref_path[256];
        snprintf(ref_path, sizeof(ref_path), ".mygit/%s", head_value + 5);

        f = fopen(ref_path, "r");
        if (f == NULL) {
            return 0;
        }

        if (fgets(out_commit, 64, f) == NULL) {
            fclose(f);
            return 0;
        }
        fclose(f);

        trim_newline(out_commit);
        return is_valid_commit_name(out_commit);
    }

    if (is_valid_commit_name(head_value)) {
        strcpy(out_commit, head_value);
        return 1;
    }

    return 0;
}

//возвращает актуальную ветку
int get_head_branch_ref(char out_ref[256]) {
    FILE* f = fopen(".mygit/HEAD.txt", "r");
    if (f == NULL) {
        return 0;
    }

    char head_value[256];
    if (fgets(head_value, sizeof(head_value), f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    trim_newline(head_value);

    if (strncmp(head_value, "ref: ", 5) != 0) {
        return 0;
    }

    strcpy(out_ref, head_value + 5);
    return 1;
}

// функция инициализирует репозиторий
void init(void) {
    int res = mkdir(".mygit", 0755);

    if (res == 0) {
        FILE* f = fopen(".mygit/add.txt", "w");
        if (f) fclose(f);

        f = fopen(".mygit/HEAD.txt", "w");
        fprintf(f, "ref: refs/heads/master");
        fclose(f);

        // после нулевого коммита следующий будет c1
        mkdir(".mygit/stage", 0755);
        mkdir(".mygit/stage/file", 0755);
        mkdir(".mygit/commits", 0755);
        mkdir(".mygit/refs", 0755);
        mkdir(".mygit/refs/heads", 0755);

        f = fopen(".mygit/refs/heads/master", "w");
        fprintf(f, "c13749284501922311454");
        fclose(f);

        // создаем нулевой коммит c0
        mkdir(".mygit/commits/c13749284501922311454", 0755);
        mkdir(".mygit/commits/c13749284501922311454/files", 0755);

        f = fopen(".mygit/commits/c13749284501922311454/file.txt", "w");
        if (f) fclose(f);

        f = fopen(".mygit/commits/c13749284501922311454/hash.txt", "w");
        if (f) fclose(f);

        f = fopen(".mygit/commits/c13749284501922311454/inf.txt", "w");
        if (f) {
            time_t now = time(NULL);
            struct tm* t = localtime(&now);

            fprintf(f, "commit=c13749284501922311454\n");
            fprintf(f, "message=initial commit\n");
            fprintf(f, "parent=none\n");
            fprintf(f, "time=%02d.%02d.%04d %02d:%02d:%02d\n",
                t->tm_mday,
                t->tm_mon + 1,
                t->tm_year + 1900,
                t->tm_hour,
                t->tm_min,
                t->tm_sec);
            fclose(f);
        }

        f = fopen(".gitignore.txt", "w");

        fprintf(f, ".gitignore.txt\n");
        fprintf(f, ".mygit\n");
        fprintf(f, "x64\n");
        fprintf(f, "main.c\n");
        fprintf(f, "dirent.h\n");
        fprintf(f, "miniGit_LegkovNV.vcxproj\n");
        fprintf(f, "miniGit_LegkovNV.vcxproj.filters\n");
        fprintf(f, "miniGit_LegkovNV.vcxproj.user\n");
        fprintf(f, "history.h\n");
        fprintf(f, "history.c\n");
        fprintf(f, "ignore.h\n");
        fprintf(f, "ignore.c\n");
        fprintf(f, "repo.c\n");
        fprintf(f, "repo.h\n");
        fprintf(f, "worktree.c\n");
        fprintf(f, "worktree.h\n");
        fprintf(f, "common.h\n");

        fclose(f);

        printf("Repository initialized\n");
    }
    else {
        if (errno == EEXIST) {
            printf("Repository already exists\n");
        }
        else {
            perror("mkdir");
        }
    }
}

//возвращает родителя полученного коммита
int get_parent_commit(const char* commit_name, char out_parent[64]) {
    char path[256];
    snprintf(path, sizeof(path), ".mygit/commits/%s/inf.txt", commit_name);

    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "parent=", 7) == 0) {
            strcpy(out_parent, line + 7);
            out_parent[strcspn(out_parent, "\r\n")] = '\0';
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

//добавление новой ветки которая будет иметь начало в нынешнем коммите
void branch_git(const char* branch_name) {
    const char* heads_dir = ".mygit/refs/heads";

    DIR* dir = opendir(heads_dir);

    struct dirent* entry;

    //выводим все существующие ветки
    if (branch_name == NULL || branch_name[0] == '\0') {
        char ref[256];
        if (get_head_branch_ref(ref)) {
           const char* branch_name = ref + 11;
            printf("%s <- we are on this branch\n", branch_name);
        }
        else {
            char commit[64];
            if (get_head_commit(commit)) {
                printf("(detached at %s <- we are on this commit and it does not point to a branch\n", commit);
            }
        }

        printf("\nAll branches in this repository:\n");

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", heads_dir, entry->d_name);

            struct stat st;
            if (stat(full_path, &st) != 0) {
                continue;
            }

            if (S_ISREG(st.st_mode)) {
                printf("%s\n", entry->d_name);
            }
        }

        closedir(dir);
        return;
    }

    //проверяем существует ли такая ветка
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (strcmp(branch_name, entry->d_name) == 0) {
            printf("fatal: branch '%s' already exists\n", branch_name);
            closedir(dir);
            return;
        }
    }

    closedir(dir);

    //добавление ветки
    char current_commit[64];
    get_head_commit(current_commit);

    char new_branch_path[512];
    snprintf(new_branch_path, sizeof(new_branch_path), "%s/%s", heads_dir, branch_name);

    FILE* f_branch = fopen(new_branch_path, "w");

    fprintf(f_branch, "%s", current_commit);
    fclose(f_branch);

    printf("branch '%s' created at %s\n", branch_name, current_commit);
}

//определяет что пришло, коммит ветка или что то непонятное
int resolve_commit_or_branch(const char* name, char out_commit[64], char out_branch[256], int* is_branch) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    if (is_valid_commit_name(name)) {
        char path[256];
        snprintf(path, sizeof(path), ".mygit/commits/%s/inf.txt", name);

        FILE* f = fopen(path, "r");
        if (f == NULL) {
            return 0;
        }
        fclose(f);

        strcpy(out_commit, name);
        if (out_branch) out_branch[0] = '\0';
        if (is_branch) *is_branch = 0;
        return 1;
    }

    char branch_path[512];
    snprintf(branch_path, sizeof(branch_path), ".mygit/refs/heads/%s", name);

    FILE* f = fopen(branch_path, "r");
    if (f == NULL) {
        return 0;
    }

    fgets(out_commit, 64, f);

    fclose(f);

    trim_newline(out_commit);

    if (!is_valid_commit_name(out_commit)) {
        return 0;
    }

    if (out_branch) {
        strcpy(out_branch, name);
    }
    if (is_branch) {
        *is_branch = 1;
    }

    return 1;
}