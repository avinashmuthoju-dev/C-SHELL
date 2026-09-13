#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "spy.h"
#include "lexer.h"

#define MAX_MEM_PATHS 512
#define PATH_BUF_LEN 4096

static int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0')
        return 0;

    for (int i = 0; s[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

static const char *get_type_str(struct stat *st)
{
    if (S_ISREG(st->st_mode))  return "REG";
    if (S_ISDIR(st->st_mode))  return "DIR";
    if (S_ISCHR(st->st_mode))  return "CHR";
    if (S_ISBLK(st->st_mode))  return "BLK";
    if (S_ISFIFO(st->st_mode)) return "FIFO";
    if (S_ISLNK(st->st_mode))  return "LNK";
    if (S_ISSOCK(st->st_mode)) return "SOCK";
    return "unknown";
}

static void print_entry(pid_t pid, const char *fd_label, const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return;

    printf("%d    %s    %s    %s\n", (int)pid, fd_label, get_type_str(&st), path);
}

static void print_symlink_entry(pid_t pid, const char *which, const char *fd_label)
{
    char linkpath[64];
    char resolved[PATH_BUF_LEN];

    snprintf(linkpath, sizeof(linkpath), "/proc/%d/%s", (int)pid, which);

    ssize_t len = readlink(linkpath, resolved, sizeof(resolved) - 1);
    if (len == -1)
        return;

    resolved[len] = '\0';
    print_entry(pid, fd_label, resolved);
}

static void print_mem_entries(pid_t pid)
{
    char mapspath[64];
    snprintf(mapspath, sizeof(mapspath), "/proc/%d/maps", (int)pid);

    FILE *fp = fopen(mapspath, "r");
    if (fp == NULL)
        return;

    char *seen[MAX_MEM_PATHS];
    int seen_count = 0;

    char line[PATH_BUF_LEN + 256];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *path = strchr(line, '/');
        if (path == NULL)
            continue; 

        size_t plen = strlen(path);
        while (plen > 0 && (path[plen - 1] == '\n' || path[plen - 1] == '\r'))
            path[--plen] = '\0';

        if (plen == 0)
            continue;

        int dup = 0;
        for (int i = 0; i < seen_count; i++)
        {
            if (strcmp(seen[i], path) == 0)
            {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        if (seen_count < MAX_MEM_PATHS)
        {
            seen[seen_count] = strdup(path);
            seen_count++;
        }

        print_entry(pid, "mem", path);
    }

    for (int i = 0; i < seen_count; i++)
        free(seen[i]);

    fclose(fp);
}


static void print_fd_entries(pid_t pid)
{
    char fddir[64];
    snprintf(fddir, sizeof(fddir), "/proc/%d/fd", (int)pid);

    DIR *dir = opendir(fddir);
    if (dir == NULL)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!is_all_digits(entry->d_name))
            continue; /* skip "." and ".." */

        char linkpath[PATH_BUF_LEN];
        char resolved[PATH_BUF_LEN];

        snprintf(linkpath, sizeof(linkpath), "%s/%s", fddir, entry->d_name);

        ssize_t len = readlink(linkpath, resolved, sizeof(resolved) - 1);
        if (len == -1)
            continue;

        resolved[len] = '\0';
        print_entry(pid, entry->d_name, resolved);
    }

    closedir(dir);
}


void exec_spy(struct token *current)
{
    current = current->next;

    pid_t target_pid;

    if (current == NULL)
    {
        /* No PID given: inspect the running shell itself. */
        target_pid = getpid();
    }
    else
    {
        if (current->next != NULL)
        {
            printf("spy: invalid syntax\n");
            return;
        }

        if (current->value == NULL || !is_all_digits(current->value))
        {
            printf("spy: invalid syntax\n");
            return;
        }

        target_pid = (pid_t)strtol(current->value, NULL, 10);
    }

    /* Verify the process exists. */
    char procdir[64];
    snprintf(procdir, sizeof(procdir), "/proc/%d", (int)target_pid);

    struct stat st;
    if (stat(procdir, &st) != 0)
    {
        printf("spy: no such process\n");
        return;
    }

    printf("PID    FD    TYPE   PATH\n");

    print_symlink_entry(target_pid, "cwd", "cwd");
    print_symlink_entry(target_pid, "exe", "txt");
    print_mem_entries(target_pid);
    print_fd_entries(target_pid);
}