#include "common.h"
#include "repo.h"
#include "history.h"
#include "worktree.h"
#include "ignore.h"

int main() {
    char line[300];

    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "init") == 0) {
            init();
        }
        else if (strcmp(line, "status") == 0) {
            status();
        }
        else if (strncmp(line, "add ", 4) == 0) {
            char path[256];
            strncpy(path, line + 4, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
            add_path(path);
        }
        else if (strncmp(line, "remove ", 7) == 0) {
            char file_name[100];
            strncpy(file_name, line + 7, sizeof(file_name) - 1);
            file_name[sizeof(file_name) - 1] = '\0';
            remove_git(file_name);
        }
        else if (strncmp(line, "commit ", 7) == 0) {
            char message[200];
            strncpy(message, line + 7, sizeof(message) - 1);
            message[sizeof(message) - 1] = '\0';
            commit(message);
        }
        else if (strcmp(line, "commit") == 0) {
            printf("fatal: The commit function without a comment has been introduced, but it is not allowed\n");
        }
        else if (strncmp(line, "log", 3) == 0) {
            if (strcmp(line, "log") == 0) {
                log_git(NULL, -1);
            }
            else {
                char arg1[128] = "";
                char arg2[128] = "";

                if (sscanf(line, "log %127s", arg1) != 1) {
                    printf("Unknown command\n");
                }
                else {
                    char* dots = strstr(arg1, "..");

                    if (dots != NULL) {
                        *dots = '\0';
                        strcpy(arg2, dots + 2);

                        if (arg1[0] == '\0' || arg2[0] == '\0') {
                            printf("Unknown command\n");
                        }
                        else {
                            log_git_for(arg1, arg2);
                        }
                    }
                    else {
                        char commit_name[20] = "";
                        char flag[10] = "";
                        char number[20] = "";

                        int cnt = sscanf(line, "log %19s %9s %19s", commit_name, flag, number);

                        if (cnt == 1) {
                            if (strcmp(commit_name, "-n") == 0) {
                                printf("fatal: missing number after -n\n");
                            }
                            else {
                                log_git(commit_name, -1);
                            }
                        }
                        else if (cnt == 2) {
                            if (strcmp(commit_name, "-n") == 0) {
                                log_git(NULL, atoi(flag));
                            }
                            else {
                                printf("Unknown command\n");
                            }
                        }
                        else if (cnt == 3) {
                            if (strcmp(flag, "-n") == 0) {
                                log_git(commit_name, atoi(number));
                            }
                            else {
                                printf("Unknown command\n");
                            }
                        }
                        else {
                            printf("Unknown command\n");
                        }
                    }
                }
            }
        }
        else if (strncmp(line, "diff", 4) == 0) {
            char c1[64] = "";
            char c2[64] = "";

            int cnt = sscanf(line, "diff %63s %63s", c1, c2);

            if (cnt == 1) {
                diff_git(c1, NULL);
            }
            else if (cnt == 2) {
                diff_git(c1, c2);
            }
            else {
                printf("Unknown command\n");
            }
        }
        else if (strncmp(line, "checkout ", 9) == 0) {
            char arg1[128] = "";
            char arg2[256] = "";

            int cnt = sscanf(line + 9, "%s %s", arg1, arg2);

            if (cnt == 1) {
                checkout_target(arg1);
            }
            else if (cnt == 2) {
                checkout_git(arg1, arg2);
            }
            else {
                printf("Unknown command\n");
            }
        }
        else if (strncmp(line, "branch", 6) == 0) {
            if ((strncmp(line, "branch ", 7) == 0)) {
                char* p = line;
                p += 7;

                while (*p == ' ') {
                    p++;
                }

                if (*p == '\0') {
                    branch_git(NULL);
                }
                else {
                    branch_git(p);
                }
            }
            else {
                branch_git(NULL);
            }
        }
        else {
            printf("Unknown command\n");
        }
    }

    return 0;
}
