#include "worktree.h"
#include "repo.h"
#include "ignore.h"

// функция копирует файл src в файл dst
int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (in == NULL) return 1;

    FILE* out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return 1;
    }

    char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }

    fclose(in);
    fclose(out);
    return 0;
}

//проверяет что это за путь папка или файл
int path_type(const char* path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0; // путь не найден
    }

    if (S_ISREG(st.st_mode)) {
        return 1; // обычный файл
    }

    if (S_ISDIR(st.st_mode)) {
        return 2; // директория
    }

    return 3; // другой тип
}

//получает адресс файла, и создает необходимые папки до этого файла
void ensure_parent_dirs(const char* file_path) {
    char tmp[512];
    strncpy(tmp, file_path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (int i = 0; tmp[i] != '\0'; i++) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            char old = tmp[i];
            tmp[i] = '\0';

            if (strlen(tmp) > 0) {
                mkdir(tmp, 0755);
            }

            tmp[i] = old;
        }
    }
}

// возвращает hash файла
void hash_file(const char* src, char out[17]) {
    FILE* file = fopen(src, "rb");

    if (file == NULL) {
        strcpy(out, "absent");
        return;
    }

    ull hash = 1469598103934665603ULL;
    unsigned char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= buffer[i];
            hash *= 1099511628211ULL;
        }
    }

    fclose(file);

    sprintf(out, "%016llx", hash);
}

//ищет хэш файла по имени файла и номеру коммита
int get_saved_hash_from_commit(char* commit_name, char* file_name1, char* out) {
    char path[512];
    snprintf(path, sizeof(path), ".mygit/commits/%s/hash.txt", commit_name);
    FILE* f_hash = fopen(path, "r");

    if (f_hash == NULL) {
        return 0;
    }

    char file_name2[256];
    char hash_data[32];

    char word[512];

    while (fscanf(f_hash, "%s", word) == 1) {
        int i = 0, j = 0;
        
        while (word[i] != '|' && word[i] != '\0') {
            file_name2[i] = word[i];
            i++;
        }

        file_name2[i] = '\0';

        i++;

        while (word[i] != '\n' && word[i] != '\0') {
            hash_data[j] = word[i];
            i++;
            j++;
        }

        hash_data[j] = '\0';

        if (strcmp(file_name1, file_name2) == 0) {
            snprintf(out, 17, "%s", hash_data);
            fclose(f_hash);
            return 1;
        }
    }

    fclose(f_hash);
    return 0;
}

//запись в stage
void stage_file_status(const char* file_name, int new_status) {
    char word[256];

    FILE* f_old = fopen(".mygit/add.txt", "r");
    FILE* f_tmp = fopen(".mygit/add_tmp.txt", "w");

    int found_in_add = 0;

    if (f_old != NULL) {
        while (fscanf(f_old, "%255s", word) == 1) {
            char name[256];
            int i = 0;

            while (word[i] != '|' && word[i] != '\0') {
                name[i] = word[i];
                i++;
            }
            name[i] = '\0';

            if (strcmp(name, file_name) == 0) {
                fprintf(f_tmp, "%s|%d\n", file_name, new_status);
                found_in_add = 1;
            }
            else {
                fprintf(f_tmp, "%s\n", word);
            }
        }
        fclose(f_old);
    }

    if (!found_in_add) {
        fprintf(f_tmp, "%s|%d\n", file_name, new_status);
    }

    fclose(f_tmp);

    remove(".mygit/add.txt");
    rename(".mygit/add_tmp.txt", ".mygit/add.txt");
}

int is_under_path(const char* base, const char* file) {
    int len = strlen(base);

    if (strcmp(base, file) == 0) {
        return 1;
    }

    if (strncmp(base, file, len) == 0 &&
        (file[len] == '/' || file[len] == '\\')) {
        return 1;
    }

    return 0;
}

// функция делает сохранение файла в момент использования
void add(const char* file_name) {
    char head_ref[256];
    if (!get_head_branch_ref(head_ref)) {
        printf("fatal: cannot commit in detached HEAD state\n");
        return;
    }

    int fl_dir = (access(file_name, 0) == 0);
    int was_in_last_commit = 0;

    char head_commit[64], old_path[256];
    get_head_commit(head_commit);

    if (get_file_path_from_commit(head_commit, file_name, old_path)) {
        was_in_last_commit = 1;
    }

    int new_status = 0;

    if (!fl_dir) {
        if (was_in_last_commit) {
            new_status = 3; // deleted
            stage_file_status(file_name, new_status);
            printf("file '%s' successfully deleted\n", file_name);
        }
        else {
            printf("fatal: pathspec '%s' did not match any files\n", file_name);
        }
        return;
    }

    if (!was_in_last_commit) {
        new_status = 1; // created
    }
    else {
        char hash_now[17], hash_old[17];

        hash_file(file_name, hash_now);
        int fl_old_hash = get_saved_hash_from_commit(head_commit, file_name, hash_old);

        if (!fl_old_hash) hash_file(old_path, hash_old);

        if (strcmp(hash_now, hash_old) == 0) {
            unstage_file(file_name);
            return; // unchanged
        }

        new_status = 2; // modified
    }

    char dst[512];
    snprintf(dst, sizeof(dst), ".mygit/stage/file/%s", file_name);

    ensure_parent_dirs(dst);

    copy_file(file_name, dst);

    stage_file_status(file_name, new_status);
    printf("file '%s' successfully added\n", file_name);
}

//
void stage_deleted_from_head_under_path(const char* path) {
    char head_commit[64];
    get_head_commit(head_commit);

    char path_head[256];
    snprintf(path_head, sizeof(path_head), ".mygit/commits/%s/file.txt", head_commit);

    FILE* f = fopen(path_head, "r");

    char word[256];

    while (fscanf(f, "%s", word) == 1) {
        char file_name[256];
        int i = 0;

        while (word[i] != '|' && word[i] != '\0') {
            file_name[i] = word[i];
            i++;
        }
        file_name[i] = '\0';

        if (!is_under_path(path, file_name)) {
            continue;
        }

        if (access(file_name, 0) != 0) {
            add(file_name);
        }
    }

    fclose(f);
}

// функция помечает, что файл удален
void remove_git(const char* file_name) {
    char head_ref[256];
    if (!get_head_branch_ref(head_ref)) {
        printf("fatal: cannot commit in detached HEAD state\n");
        return;
    }

    char word[100];

    int was_in_last_commit = 0;
    char head_commit[64];
    get_head_commit(head_commit);

    char path_head[256];
    snprintf(path_head, sizeof(path_head), ".mygit/commits/%s/file.txt", head_commit);

    FILE* f_last = fopen(path_head, "r");

    while (fscanf(f_last, "%99s", word) == 1) {
        char name[100];
        int i = 0;

        while (word[i] != '|' && word[i] != '\0') {
            name[i] = word[i];
            i++;
        }
        name[i] = '\0';

        if (strcmp(name, file_name) == 0) {
            was_in_last_commit = 1;
            break;
        }
    }
    fclose(f_last);

    if (!was_in_last_commit) {
        printf("fatal: pathspec '%s' did not match any file in last commit\n", file_name);
        return;
    }

    FILE* f_old = fopen(".mygit/add.txt", "r");
    FILE* f_tmp = fopen(".mygit/add_tmp.txt", "w");

    if (f_tmp == NULL) {
        printf("fatal: cannot open add_tmp.txt\n");
        if (f_old) fclose(f_old);
        return;
    }

    int found_in_add = 0;

    if (f_old != NULL) {
        while (fscanf(f_old, "%99s", word) == 1) {
            char name[100];
            int i = 0;

            while (word[i] != '|' && word[i] != '\0') {
                name[i] = word[i];
                i++;
            }
            name[i] = '\0';

            if (strcmp(name, file_name) == 0) {
                fprintf(f_tmp, "%s|3\n", file_name);
                found_in_add = 1;
            }
            else {
                fprintf(f_tmp, "%s\n", word);
            }
        }
        fclose(f_old);
    }

    if (!found_in_add) {
        fprintf(f_tmp, "%s|3\n", file_name);
    }

    fclose(f_tmp);

    remove(".mygit/add.txt");
    rename(".mygit/add_tmp.txt", ".mygit/add.txt");

    printf("file '%s' staged for deletion\n", file_name);
}

// выводит нынешний статус коммита + невощедщие файлы
void status(void) {
    FILE* f = fopen(".mygit/add.txt", "r");
    if (f == NULL) {
        printf("Repository not initialized\n");
        return;
    }

    char word[128];

    while (fscanf(f, "%127s", word) == 1) {
        char* p = strchr(word, '|');

        if (p == NULL) {
            continue;
        }

        *p = '\0';
        p++;

        printf("%s - ", word);

        if (*p == '1') {
            printf("\x1b[32mcreated\x1b[0m\n");
        }
        else if (*p == '2') {
            printf("\x1b[34mmodified\x1b[0m\n");
        }
        else if (*p == '3') {
            printf("\x1b[31mdeleted\x1b[0m\n");
        }
        else {
            printf("unknown\n");
        }
    }

    fclose(f);

    status_untracked_files(".");
    status_deleted_files();
}

//файлы которые есть в директории, но не добавленный в stage и при этом по сравнению с прошлым коммитом обновлены
void status_untracked_files(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        printf("Cannot open directory: %s\n", path);
        return;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        }
        else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }

        if (is_ignored(full_path) || is_ignored(entry->d_name)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            status_untracked_files(full_path);
        }
        else if (S_ISREG(st.st_mode)) {
            FILE* f_add = fopen(".mygit/add.txt", "r");

            char word[128];
            int in_add = 0;

            while (fscanf(f_add, "%127s", word) == 1) {
                char* p = strchr(word, '|');

                if (p == NULL) {
                    continue;
                }

                *p = '\0';

                if (strcmp(full_path, word) == 0) {
                    in_add = 1;
                    break;
                }
            }

            fclose(f_add);

            if (in_add) {
                continue;
            }

            char head_commit[64];
            if (!get_head_commit(head_commit)) {
                closedir(dir);
                return;
            }

            char old_path[512];
            if (!get_file_path_from_commit(head_commit, full_path, old_path)) {
                printf("%s - \x1b[32mcreated but not stage\x1b[0m\n", full_path);
                continue;
            }

            char hash_now[17], hash_old[17];
            hash_file(full_path, hash_now);
            int fl_old_hash = get_saved_hash_from_commit(head_commit, full_path, hash_old);

            if (!fl_old_hash) hash_file(old_path, hash_old);

            if (strcmp(hash_now, hash_old) != 0) {
                printf("%s - \x1b[34mmodified but not staged\x1b[0m\n", full_path);
            }
        }
    }

    closedir(dir);
}

//так же не добавленные в stage, но при этом есть в прошлом коммите, но в нынешней директории их нету
void status_deleted_files(void) {
    char head_commit[64];
    if (!get_head_commit(head_commit)) {
        printf("fatal: cannot resolve HEAD\n");
        return;
    }

    char path_head[256];
    snprintf(path_head, sizeof(path_head), ".mygit/commits/%s/file.txt", head_commit);

    FILE* f_last = fopen(path_head, "r");

    char word[256];

    while (fscanf(f_last, "%255s", word) == 1) {
        char file_name[256];
        int i = 0;

        while (word[i] != '|' && word[i] != '\0') {
            file_name[i] = word[i];
            i++;
        }
        file_name[i] = '\0';

        if (access(file_name, 0) == 0) {
            continue;
        }

        int in_add = 0;
        FILE* f_add = fopen(".mygit/add.txt", "r");
        if (f_add != NULL) {
            char add_word[256];

            while (fscanf(f_add, "%255s", add_word) == 1) {
                char tmp[256];
                strcpy(tmp, add_word);

                char* p = strchr(tmp, '|');
                if (p != NULL) {
                    *p = '\0';
                }

                if (strcmp(file_name, tmp) == 0) {
                    in_add = 1;
                    break;
                }
            }

            fclose(f_add);
        }

        if (!in_add) {
            printf("%s - \x1b[31mdeleted but not staged\x1b[0m\n", file_name);
        }
    }

    fclose(f_last);
}

//берет файл из указанного коммита и записывает в рабочую дирикторию
void checkout_git(const char* commit_name, const char* file_name) {
    char resolved_commit[64];
    char branch_name[256];
    int is_branch = 0;
    char src_path[256];

    if (!resolve_commit_or_branch(commit_name, resolved_commit, branch_name, &is_branch)) {
        printf("fatal: unknown commit or branch '%s'\n", commit_name);
        return;
    }

    if (!get_file_path_from_commit(resolved_commit, file_name, src_path)) {
        printf("fatal: file '%s' not found in '%s'\n", file_name, commit_name);
        return;
    }

    ensure_parent_dirs(file_name);

    if (copy_file(src_path, file_name) != 0) {
        printf("fatal: cannot restore file '%s' from commit '%s'\n", file_name, commit_name);
        return;
    }

    printf("file '%s' restored from commit '%s'\n", file_name, commit_name);
}

//обработка add директории
void walk_directory_and_add(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        printf("Cannot open directory: %s\n", path);
        return;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];

        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        }
        else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }

        if (is_ignored(full_path) || is_ignored(entry->d_name)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            walk_directory_and_add(full_path);
        }
        else if (S_ISREG(st.st_mode)) {
            add(full_path);
        }
    }

    closedir(dir);
}

//принимает путь и обрабатывает его
void add_path(const char* path) {
    int t = path_type(path);

    if (t == 1) {
        add(path);
    }
    else if (t == 2) {
        walk_directory_and_add(path);
        stage_deleted_from_head_under_path(path);
    }
    else if (t == 0) {
        add(path);
    }
    else {
        printf("fatal: unsupported path '%s'\n", path);
    }
}

// удаление файла из stage
void unstage_file(const char* file_name) {
    char word[256];

    FILE* f_old = fopen(".mygit/add.txt", "r");
    FILE* f_tmp = fopen(".mygit/add_tmp.txt", "w");

    if (f_tmp == NULL) {
        if (f_old) fclose(f_old);
        return;
    }

    if (f_old != NULL) {
        while (fscanf(f_old, "%255s", word) == 1) {
            char name[256];
            int i = 0;

            while (word[i] != '|' && word[i] != '\0') {
                name[i] = word[i];
                i++;
            }
            name[i] = '\0';

            if (strcmp(name, file_name) != 0) {
                fprintf(f_tmp, "%s\n", word);
            }
        }
        fclose(f_old);
    }

    fclose(f_tmp);

    remove(".mygit/add.txt");
    rename(".mygit/add_tmp.txt", ".mygit/add.txt");
}

//удаляет все файлы
void del_all_file_dir(const char* path) {
    DIR* dir = opendir(path);

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (is_ignored(full_path) || is_ignored(entry->d_name)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            del_all_file_dir(full_path);
            rmdir(full_path);
        }
        else if (S_ISREG(st.st_mode)) {
            remove(full_path);
        }
    }

    closedir(dir);
}

void checkout_all_file_commit(const char* name) {
    char commit_name[64];
    char branch_name[256] = "";
    int from_branch = 0;

    if (!resolve_commit_or_branch(name, commit_name, branch_name, &from_branch)) {
        printf("fatal: unknown commit or branch '%s'\n", name);
        return;
    }

    if (!working_tree_is_clean()) {
        printf("fatal: local changes would be overwritten by checkout\n");
        return;
    }

    char path[256], word[256];
    snprintf(path, sizeof(path), ".mygit/commits/%s/file.txt", commit_name);

    FILE* f_commit = fopen(path, "r");
    if (f_commit == NULL) {
        printf("fatal: cannot open commit '%s'\n", commit_name);
        return;
    }

    del_all_file_dir(".");

    while (fscanf(f_commit, "%s", word) == 1) {
        int i = 0;

        while (word[i] != '|' && word[i] != '\0') {
            i++;
        }

        word[i] = '\0';

        checkout_git(commit_name, word);
    }

    fclose(f_commit);

    if (from_branch) {
        FILE* f_head = fopen(".mygit/HEAD.txt", "w");
        fprintf(f_head, "ref: refs/heads/%s", branch_name);
        fclose(f_head);
        printf("Switched to branch '%s'\n", branch_name);
    }
    else {
        FILE* f_head = fopen(".mygit/HEAD.txt", "w");
        fprintf(f_head, "%s", commit_name);
        fclose(f_head);
        printf("HEAD is now at %s (detached)\n", commit_name);
    }

    FILE* clear = fopen(".mygit/add.txt", "w");
    if (clear) fclose(clear);
}

//вспомогательная для проверки на чистоту
int has_untracked_files_recursive(const char* path, const char* head_commit) {
    DIR* dir = opendir(path);
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        }
        else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }

        if (is_ignored(full_path) || is_ignored(entry->d_name)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (has_untracked_files_recursive(full_path, head_commit)) {
                closedir(dir);
                return 1;
            }
        }
        else if (S_ISREG(st.st_mode)) {
            char tmp[256];
            if (!get_file_path_from_commit(head_commit, full_path, tmp)) {
                closedir(dir);
                return 1;
            }
        }
    }

    closedir(dir);
    return 0;
}

//проверяет были ли какие то изменения относительно последнего коммита
int working_tree_is_clean(void) {
    FILE* f = fopen(".mygit/add.txt", "r");
    if (f == NULL) {
        return 0;
    }

    char word[256];
    if (fscanf(f, "%255s", word) == 1) {
        fclose(f);
        return 0;
    }
    fclose(f);

    char head_commit[64];
    if (!get_head_commit(head_commit)) {
        return 0;
    }

    char path_head[256];
    snprintf(path_head, sizeof(path_head), ".mygit/commits/%s/file.txt", head_commit);

    FILE* f_head = fopen(path_head, "r");
    if (f_head == NULL) {
        return 0;
    }

    while (fscanf(f_head, "%255s", word) == 1) {
        char file_name[256];
        int i = 0;

        while (word[i] != '|' && word[i] != '\0') {
            file_name[i] = word[i];
            i++;
        }
        file_name[i] = '\0';

        char old_path[512];
        if (!get_file_path_from_commit(head_commit, file_name, old_path)) {
            fclose(f_head);
            return 0;
        }

        if (access(file_name, 0) != 0) {
            fclose(f_head);
            return 0;
        }

        char hash_now[17], hash_old[17];
        hash_file(file_name, hash_now);
        hash_file(old_path, hash_old);

        if (strcmp(hash_now, hash_old) != 0) {
            fclose(f_head);
            return 0;
        }
    }

    fclose(f_head);

    if (has_untracked_files_recursive(".", head_commit)) {
        return 0;
    }

    return 1;
}

void checkout_target(const char* name) {
    checkout_all_file_commit(name);
}