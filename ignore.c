#include "ignore.h"

//функция проверяет нужно ли игнорировать файл или нет, что именно игнорируется решает пользователь
int is_ignored(const char* path) {
    FILE* f_ignor = fopen("./.gitignore.txt", "r");

    if (f_ignor == NULL) {
        return 0;
    }

    char word[128];

    while (fscanf(f_ignor, "%s", word) == 1) {
        if (strcmp(word, path) == 0) {
            fclose(f_ignor);
            return 1;
        }
    }

    fclose(f_ignor);

    return 0;
}