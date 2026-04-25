#include "history.h"
#include "repo.h"
#include "worktree.h"

//hash коммита
typedef unsigned long long ull;

static ull fnv1a_update_ull(ull hash, const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        hash ^= (ull)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static ull generate_commit_id(const char* parent_commit, const char* message, const struct tm* t) {
    ull hash = 1469598103934665603ULL;

    if (parent_commit != NULL) {
        hash = fnv1a_update_ull(hash, (const unsigned char*)parent_commit, strlen(parent_commit));
    }

    if (message != NULL) {
        hash = fnv1a_update_ull(hash, (const unsigned char*)message, strlen(message));
    }

    char time_buf[64];
    snprintf(time_buf, sizeof(time_buf),
        "%04d-%02d-%02d %02d:%02d:%02d",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec);

    hash = fnv1a_update_ull(hash, (const unsigned char*)time_buf, strlen(time_buf));

    if (hash == 0) {
        hash = 1;
    }

    return hash;
}

//сравнивает коммиты
void diff_git(const char* commit_name1, const char* commit_name2) {
    char resolved1[64], resolved2[64], br[256];
    int is_branch = 0;

    if (!resolve_commit_or_branch(commit_name1, resolved1, br, &is_branch)) {
        printf("fatal: unknown commit or branch '%s'\n", commit_name1);
        return;
    }

    if (commit_name2 == NULL) {
        if (!get_head_commit(resolved2)) {
            printf("fatal: cannot resolve HEAD\n");
            return;
        }
    }
    else {
        if (!resolve_commit_or_branch(commit_name2, resolved2, br, &is_branch)) {
            printf("fatal: unknown commit or branch '%s'\n", commit_name2);
            return;
        }
    }

    char path_1[256], path_2[256];

    snprintf(path_1, sizeof(path_1), ".mygit/commits/%s/file.txt", resolved1);
    snprintf(path_2, sizeof(path_2), ".mygit/commits/%s/file.txt", resolved2);

    FILE* f1 = fopen(path_1, "r");

    if (f1 == NULL) {
        printf("fatal: cannot open commit '%s'\n", resolved1);
        return;
    }

    char buffer1[128];

    if (strcmp(resolved1, resolved2) == 0) {
        printf("No differences found\n");
        fclose(f1);
        return;
    }

    printf("diff %s %s\n\n", resolved1, resolved2);

    while (fscanf(f1, "%127s", buffer1) == 1) {
        char file_name[256];

        int i = 0;
        while (buffer1[i] != '|' && buffer1[i] != '\0') {
            file_name[i] = buffer1[i];
            i++;
        }
        file_name[i] = '\0';

        int k1 = 0, k2 = 0;
        char path1[512], path2[512];

        if (get_file_path_from_commit(resolved1, file_name, path1)) {
            k1 = 1;
        }
        if (get_file_path_from_commit(resolved2, file_name, path2)) {
            k2 = 1;
        }

        if (k1 && k2) {
            char hash1[17], hash2[17];

            int fl_hash_1 = get_saved_hash_from_commit(resolved1, file_name, hash1);
            int fl_hash_2 = get_saved_hash_from_commit(resolved2, file_name, hash2);

            if (!fl_hash_1) hash_file(path1, hash1);
            if (!fl_hash_2) hash_file(path2, hash2);

            if (strcmp(hash1, hash2) != 0) {
                printf("%s\n", file_name);
                printf("  %s: %s\n", resolved1, hash1);
                printf("  %s: %s\n", resolved2, hash2);
                ldc(path1, path2);
                printf("\n");
            }
        }
        else if (k1 && !k2) {
            char hash1[17];
            int fl_hash_1 = get_saved_hash_from_commit(resolved1, file_name, hash1);

            if (!fl_hash_1) hash_file(path1, hash1);

            printf("%s\n", file_name);
            printf("  %s: %s\n", resolved1, hash1);
            printf("  %s: absent\n", resolved2);
            ldc(path1, NULL);
            printf("\n");
        }
    }

    fclose(f1);

    FILE* f2 = fopen(path_2, "r");

    if (f2 == NULL) {
        printf("fatal: cannot open commit '%s'\n", resolved2);
        return;
    }

    char buffer2[256];

    while (fscanf(f2, "%255s", buffer2) == 1) {
        char file_name[256];

        int i = 0;
        while (buffer2[i] != '|' && buffer2[i] != '\0') {
            file_name[i] = buffer2[i];
            i++;
        }
        file_name[i] = '\0';

        char path1[512], path2[512];

        if (get_file_path_from_commit(resolved1, file_name, path1)) {
            continue;
        }
        if (get_file_path_from_commit(resolved2, file_name, path2)) {
            char hash2[17];
            int fl_hash_2 = get_saved_hash_from_commit(resolved2, file_name, hash2);

            if (!fl_hash_2) hash_file(path2, hash2);

            printf("%s\n", file_name);
            printf("  %s: absent\n", resolved1);
            printf("  %s: %s\n", resolved2, hash2);
            ldc(NULL, path2);
            printf("\n");
        }
    }

    fclose(f2);
}

// сохранение нынешней версии проекта
void commit(const char* message) {
    char path[512];

    char head_commit[64];
    if (!get_head_commit(head_commit)) {
        printf("fatal: cannot resolve HEAD\n");
        return;
    }

    char head_ref[256];
    if (!get_head_branch_ref(head_ref)) {
        printf("fatal: cannot commit in detached HEAD state\n");
        return;
    }

    FILE* check = fopen(".mygit/add.txt", "r");
    if (check == NULL) {
        printf("Repository not initialized\n");
        return;
    }

    char test[256];
    if (fscanf(check, "%255s", test) != 1) {
        printf("nothing to commit\n");
        fclose(check);
        return;
    }
    fclose(check);

    /* вычисл€ем название нового коммита */
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t == NULL) {
        printf("fatal: cannot get local time\n");
        return;
    }

    ull number_commit = generate_commit_id(head_commit, message, t);

    now = time(NULL);
    t = localtime(&now);
    if (t == NULL) {
        printf("fatal: cannot get local time\n");
        return;
    }

    /* создаем директории нового коммита */
    snprintf(path, sizeof(path), ".mygit/commits/c%llu", number_commit);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), ".mygit/commits/c%llu/files", number_commit);
    mkdir(path, 0755);

    /* открываем файлы нового коммита */
    char path_file[512];
    char path_inf[512];
    char path_hash_new[512];
    char path_head[512];

    snprintf(path_file, sizeof(path_file), ".mygit/commits/c%llu/file.txt", number_commit);
    snprintf(path_inf, sizeof(path_inf), ".mygit/commits/c%llu/inf.txt", number_commit);
    snprintf(path_hash_new, sizeof(path_hash_new), ".mygit/commits/c%llu/hash.txt", number_commit);
    snprintf(path_head, sizeof(path_head), ".mygit/commits/%s/file.txt", head_commit);

    FILE* f_file = fopen(path_file, "w");
    FILE* f_inf = fopen(path_inf, "w");
    FILE* f_hash = fopen(path_hash_new, "w");
    FILE* f_last = fopen(path_head, "r");
    FILE* f_new = fopen(".mygit/add.txt", "r");

    if (f_file == NULL || f_inf == NULL || f_hash == NULL || f_new == NULL) {
        printf("fatal: cannot create commit files\n");
        if (f_file) fclose(f_file);
        if (f_inf) fclose(f_inf);
        if (f_hash) fclose(f_hash);
        if (f_last) fclose(f_last);
        if (f_new) fclose(f_new);
        return;
    }

    /* записываем информацию о коммите */
    fprintf(f_inf, "commit=c%llu\n", number_commit);
    fprintf(f_inf, "message=%s\n", message);
    fprintf(f_inf, "parent=%s\n", head_commit);
    fprintf(f_inf, "time=%02d.%02d.%04d %02d:%02d:%02d\n",
        t->tm_mday,
        t->tm_mon + 1,
        t->tm_year + 1900,
        t->tm_hour,
        t->tm_min,
        t->tm_sec);
    fclose(f_inf);

    char word_l[256];
    char word_n[256];

    /* переносим staged изменени€ в новый коммит */
    while (fscanf(f_new, "%255s", word_n) == 1) {
        char file_name[256];
        int i = 0;

        while (word_n[i] != '|' && word_n[i] != '\0' && i < (int)sizeof(file_name) - 1) {
            file_name[i] = word_n[i];
            i++;
        }
        file_name[i] = '\0';

        char status = word_n[strlen(word_n) - 1];

        /* файл удален Ч просто не переносим его в новый коммит */
        if (status == '3') {
            continue;
        }

        fprintf(f_file, "%s|c%llu\n", file_name, number_commit);

        char path1[512];
        char path2[512];

        snprintf(path1, sizeof(path1), ".mygit/commits/c%llu/files/%s", number_commit, file_name);
        snprintf(path2, sizeof(path2), ".mygit/stage/file/%s", file_name);

        ensure_parent_dirs(path1);

        if (copy_file(path2, path1) != 0) {
            printf("fatal: cannot copy staged file '%s'\n", file_name);
            fclose(f_new);
            if (f_last) fclose(f_last);
            fclose(f_file);
            fclose(f_hash);
            return;
        }

        char out[17];
        hash_file(path2, out);
        fprintf(f_hash, "%s|%s\n", file_name, out);
    }
    fclose(f_new);

    /* добавл€ем в новый коммит файлы из родител€, которых нет в add.txt */
    if (f_last != NULL) {
        while (fscanf(f_last, "%255s", word_l) == 1) {
            FILE* f_check = fopen(".mygit/add.txt", "r");
            int fl = 1;

            char old_name[256];
            int i = 0;
            while (word_l[i] != '|' && word_l[i] != '\0' && i < (int)sizeof(old_name) - 1) {
                old_name[i] = word_l[i];
                i++;
            }
            old_name[i] = '\0';

            if (f_check != NULL) {
                while (fscanf(f_check, "%255s", word_n) == 1) {
                    if (strncmp(word_n, old_name, strlen(old_name)) == 0 &&
                        word_n[strlen(old_name)] == '|') {
                        fl = 0;
                        break;
                    }
                }
                fclose(f_check);
            }

            if (fl) {
                fprintf(f_file, "%s\n", word_l);
            }
        }

        fclose(f_last);
    }

    fclose(f_file);
    fclose(f_hash);

    /* обновл€ем указатель ветки */
    char branch_path[512];
    snprintf(branch_path, sizeof(branch_path), ".mygit/%s", head_ref);

    FILE* dst = fopen(branch_path, "w");
    if (dst == NULL) {
        printf("fatal: cannot update branch head\n");
        return;
    }

    fprintf(dst, "c%llu", number_commit);
    fclose(dst);

    /* очищаем stage */
    FILE* clear = fopen(".mygit/add.txt", "w");
    if (clear) fclose(clear);

    printf("commit c%llu created, message: '%s'\n", number_commit, message);
}

//чтение строк из файлов дл€ удобства
int read_lines_from_file(FILE* f, char*** out_lines, int* out_count) {
    *out_lines = NULL;
    *out_count = 0;

    int cap = 0;
    char line[1024];

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';

        if (*out_count == cap) {
            cap = (cap == 0) ? 8 : cap * 2;

            char** tmp = realloc(*out_lines, cap * sizeof(char*));
            if (tmp == NULL) {
                for (int i = 0; i < *out_count; i++) {
                    free((*out_lines)[i]);
                }
                free(*out_lines);
                *out_lines = NULL;
                *out_count = 0;
                return 0;
            }

            *out_lines = tmp;
        }

        (*out_lines)[*out_count] = malloc(strlen(line) + 1);
        if ((*out_lines)[*out_count] == NULL) {
            for (int i = 0; i < *out_count; i++) {
                free((*out_lines)[i]);
            }
            free(*out_lines);
            *out_lines = NULL;
            *out_count = 0;
            return 0;
        }

        strcpy((*out_lines)[*out_count], line);
        (*out_count)++;
    }

    return 1;
}

//динамический алгоритм сравнени€ файлов
void ldc(char* path1, char* path2) {
    FILE* f1 = NULL;
    FILE* f2 = NULL;

    if (path1 != NULL) {
        f1 = fopen(path1, "r");
    }
    if (path2 != NULL) {
        f2 = fopen(path2, "r");
    }

    char** a = NULL;
    char** b = NULL;
    int n = 0, m = 0;

    if (f1 != NULL) {
        read_lines_from_file(f1, &a, &n);
        fclose(f1);
    }

    if (f2 != NULL) {
        read_lines_from_file(f2, &b, &m);
        fclose(f2);
    }

    int** dp = malloc((n + 1) * sizeof(int*));
    for (int i = 0; i <= n; i++) {
        dp[i] = calloc(m + 1, sizeof(int));
    }

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            if (strcmp(a[i - 1], b[j - 1]) == 0) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            }
            else {
                dp[i][j] = (dp[i - 1][j] > dp[i][j - 1]) ? dp[i - 1][j] : dp[i][j - 1];
            }
        }
    }

    char** ans = malloc((n + m + 1) * sizeof(char*));
    int cnt = 0, i = n, j = m;

    while (i > 0 && j > 0) {
        if (strcmp(a[i - 1], b[j - 1]) == 0) {
            i--;
            j--;
        }
        else if (dp[i - 1][j] >= dp[i][j - 1]) {
            char* s = malloc(strlen(a[i - 1]) + 20);
            sprintf(s, "\x1b[31m- %s\x1b[0m\n", a[i - 1]);
            ans[cnt++] = s;
            i--;
        }
        else {
            char* s = malloc(strlen(b[j - 1]) + 20);
            sprintf(s, "\x1b[32m+ %s\x1b[0m\n", b[j - 1]);
            ans[cnt++] = s;
            j--;
        }
    }

    while (i > 0) {
        char* s = malloc(strlen(a[i - 1]) + 20);
        sprintf(s, "\x1b[31m- %s\x1b[0m\n", a[i - 1]);
        ans[cnt++] = s;
        i--;
    }

    while (j > 0) {
        char* s = malloc(strlen(b[j - 1]) + 20);
        sprintf(s, "\x1b[32m+ %s\x1b[0m\n", b[j - 1]);
        ans[cnt++] = s;
        j--;
    }

    for (int k = cnt - 1; k >= 0; k--) {
        printf("%s", ans[k]);
        free(ans[k]);
    }

    free(ans);

    for (int x = 0; x <= n; x++) {
        free(dp[x]);
    }
    free(dp);

    for (int x = 0; x < n; x++) {
        free(a[x]);
    }
    free(a);

    for (int x = 0; x < m; x++) {
        free(b[x]);
    }
    free(b);
}

// выводит логи коммитов
void log_git(const char* commit_name, int num_commit_log) {
    char current_commit[64];

    if (commit_name == NULL) {
        if (!get_head_commit(current_commit)) {
            printf("fatal: cannot resolve HEAD\n");
            return;
        }
    }
    else {
        char branch_name[256];
        int is_branch = 0;

        if (!resolve_commit_or_branch(commit_name, current_commit, branch_name, &is_branch)) {
            printf("fatal: unknown commit or branch '%s'\n", commit_name);
            return;
        }
    }

    int printed = 0;

    while (1) {
        if (num_commit_log != -1 && printed >= num_commit_log) {
            return;
        }

        char commit_file_path[200];
        snprintf(commit_file_path, sizeof(commit_file_path), ".mygit/commits/%s/inf.txt", current_commit);

        FILE* f_inf = fopen(commit_file_path, "r");
        if (f_inf == NULL) {
            return;
        }

        printf("commit %s\n", current_commit);

        char line[256];
        int num_f = 0;

        while (fgets(line, sizeof(line), f_inf) != NULL) {
            int fl = 0, line_i = 0;

            num_f++;

            if (num_f == 1) {
                continue;
            }

            while (1) {
                if (line[line_i] == '\0') {
                    break;
                }
                else if (line[line_i] == '=' && fl == 0) {
                    printf(": ");
                    fl = 1;
                }
                else {
                    printf("%c", line[line_i]);
                }

                line_i++;
            }
        }

        fclose(f_inf);
        printf("\n");
        printed++;

        char parent[64];
        if (!get_parent_commit(current_commit, parent)) {
            return;
        }

        if (strcmp(parent, "none") == 0) {
            return;
        }

        strcpy(current_commit, parent);
    }
}

void log_git_for(const char* commit_1, const char* commit_2) {
    char resolved1[64], resolved2[64], branch_name[256];
    int is_branch = 0;

    if (!resolve_commit_or_branch(commit_1, resolved1, branch_name, &is_branch)) {
        printf("fatal: unknown commit or branch '%s'\n", commit_1);
        return;
    }

    if (!resolve_commit_or_branch(commit_2, resolved2, branch_name, &is_branch)) {
        printf("fatal: unknown commit or branch '%s'\n", commit_2);
        return;
    }

    char current_commit[64];
    strcpy(current_commit, resolved2);

    while (1) {
        char commit_file_path[200];
        snprintf(commit_file_path, sizeof(commit_file_path), ".mygit/commits/%s/inf.txt", current_commit);

        FILE* f_inf = fopen(commit_file_path, "r");
        if (f_inf == NULL) {
            return;
        }

        printf("commit %s\n", current_commit);

        char line[256];
        int num_f = 0;

        while (fgets(line, sizeof(line), f_inf) != NULL) {
            int fl = 0, line_i = 0;
            num_f++;

            if (num_f == 1) {
                continue;
            }

            while (1) {
                if (line[line_i] == '\0') {
                    break;
                }
                else if (line[line_i] == '=' && fl == 0) {
                    printf(": ");
                    fl = 1;
                }
                else {
                    printf("%c", line[line_i]);
                }

                line_i++;
            }
        }

        fclose(f_inf);
        printf("\n");

        if (strcmp(current_commit, resolved1) == 0) {
            return;
        }

        char parent[64];
        if (!get_parent_commit(current_commit, parent)) {
            return;
        }

        if (strcmp(parent, "none") == 0) {
            printf("fatal: commit '%s' is not an ancestor of '%s'\n", commit_1, commit_2);
            return;
        }

        strcpy(current_commit, parent);
    }
}