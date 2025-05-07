#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

void walk_dir(const char *dir_path, int show_links, int show_dirs, int show_files, int sort);

int str_cmp(const void *a, const void *b) {
    return strcoll(*(const char **)a, *(const char **)b);
}

void print_filtered(const char *path, const struct stat *st, int show_links, int show_dirs, int show_files) {
    if ((show_links && S_ISLNK(st->st_mode)) ||
        (show_dirs && S_ISDIR(st->st_mode)) ||
        (show_files && S_ISREG(st->st_mode))) {
        printf("%s\n", path);
    }
}

void walk_dir(const char *dir_path, int show_links, int show_dirs, int show_files, int sort) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror(dir_path);
        return;
    }

    struct dirent *entry;
    char **paths = NULL;
    size_t count = 0, capacity = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) {
            perror(full_path);
            continue;
        }

        int is_match = (show_links && S_ISLNK(st.st_mode)) ||
                       (show_dirs && S_ISDIR(st.st_mode)) ||
                       (show_files && S_ISREG(st.st_mode));

        if (sort && is_match) {
            if (count >= capacity) {
                capacity = capacity ? capacity * 2 : 32;
                paths = realloc(paths, capacity * sizeof(char *));
                if (!paths) {
                    perror("realloc");
                    closedir(dir);
                    return;
                }
            }
            paths[count++] = strdup(full_path);
        } else if (!sort) {
            print_filtered(full_path, &st, show_links, show_dirs, show_files);
        }

        // Рекурсивно обходим директории
        if (S_ISDIR(st.st_mode)) {
            walk_dir(full_path, show_links, show_dirs, show_files, sort);
        }
    }

    closedir(dir);

    if (sort && count > 0) {
        qsort(paths, count, sizeof(char *), str_cmp);
        for (size_t i = 0; i < count; ++i) {
            struct stat st;
            if (lstat(paths[i], &st) == -1) {
                perror(paths[i]);
                free(paths[i]);
                continue;
            }
            print_filtered(paths[i], &st, show_links, show_dirs, show_files);

            // рекурсивный обход директорий после сортировки
            if (S_ISDIR(st.st_mode)) {
                walk_dir(paths[i], show_links, show_dirs, show_files, sort);
            }

            free(paths[i]);
        }
        free(paths);
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_COLLATE, "");

    int show_links = 0, show_dirs = 0, show_files = 0, sort = 0;
    int opt;

    while ((opt = getopt(argc, argv, "ldfs")) != -1) {
        switch (opt) {
            case 'l': show_links = 1; break;
            case 'd': show_dirs = 1; break;
            case 'f': show_files = 1; break;
            case 's': sort = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-d] [-f] [-s] [directory]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!show_links && !show_dirs && !show_files) {
        show_links = show_dirs = show_files = 1;
    }

    const char *start_dir = (optind < argc) ? argv[optind] : ".";

    struct stat st;
    if (lstat(start_dir, &st) == 0 && S_ISDIR(st.st_mode) && show_dirs) {
        printf("%s\n", start_dir);
    }

    walk_dir(start_dir, show_links, show_dirs, show_files, sort);
    return 0;
}
