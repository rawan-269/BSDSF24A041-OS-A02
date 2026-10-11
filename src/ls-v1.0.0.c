/*
* Programming Assignment 02: lsv1.1.0
* Feature added: -l option (long listing format)
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls -l /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>     // getpwuid()
#include <grp.h>     // getgrgid()
#include <time.h>    // ctime(), time()

#define SIX_MONTHS (6L * 30 * 24 * 60 * 60)   // seconds in about 6 months

extern int errno;

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char str[]);
int num_len(long n);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    // getopt() checks the arguments one by one for options like -l
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        if (opt == 'l')
            long_flag = 1;
        else
        {
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(1);
        }
    }

    // optind = index of first argument that is NOT an option
    if (optind == argc)        // no directory given
    {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

/* Simple listing (same as version 1.0.0) */
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* Long listing: like ls -l */
void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat info;
    char path[1024];
    char user[64], group[64];

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* ---------- PASS 1: find column widths and total blocks ---------- */
    int w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    long total_blocks = 0;

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        // make full path: dir/filename
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &info) == -1)
            continue;

        total_blocks += info.st_blocks;

        int len = num_len(info.st_nlink);
        if (len > w_links) w_links = len;

        struct passwd *pw = getpwuid(info.st_uid);
        if (pw != NULL)
            snprintf(user, sizeof(user), "%s", pw->pw_name);
        else
            snprintf(user, sizeof(user), "%d", info.st_uid);
        len = strlen(user);
        if (len > w_user) w_user = len;

        struct group *gr = getgrgid(info.st_gid);
        if (gr != NULL)
            snprintf(group, sizeof(group), "%s", gr->gr_name);
        else
            snprintf(group, sizeof(group), "%d", info.st_gid);
        len = strlen(group);
        if (len > w_group) w_group = len;

        len = num_len(info.st_size);
        if (len > w_size) w_size = len;
    }

    // st_blocks counts 512-byte blocks, ls shows 1024-byte blocks
    printf("total %ld\n", total_blocks / 2);

    /* ---------- PASS 2: print each file ---------- */
    rewinddir(dp);    // go back to the start of the directory

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &info) == -1)
        {
            perror(path);
            continue;
        }

        // 1. permissions string
        char perms[11];
        mode_to_string(info.st_mode, perms);

        // 2. owner and group names
        struct passwd *pw = getpwuid(info.st_uid);
        if (pw != NULL)
            snprintf(user, sizeof(user), "%s", pw->pw_name);
        else
            snprintf(user, sizeof(user), "%d", info.st_uid);

        struct group *gr = getgrgid(info.st_gid);
        if (gr != NULL)
            snprintf(group, sizeof(group), "%s", gr->gr_name);
        else
            snprintf(group, sizeof(group), "%d", info.st_gid);

        // 3. time: ctime() gives "Wed Jun 30 21:49:08 1993\n"
        char *t = ctime(&info.st_mtime);
        char timestr[20];
        time_t now = time(NULL);

        if (now - info.st_mtime > SIX_MONTHS || info.st_mtime > now)
            snprintf(timestr, sizeof(timestr), "%.6s  %.4s", t + 4, t + 20); // old file: "Jun 30  1993"
        else
            snprintf(timestr, sizeof(timestr), "%.12s", t + 4);            // new file: "Jun 30 21:49"

        // 4. print one line
        printf("%s %*ld %-*s %-*s %*ld %s %s",
               perms,
               w_links, (long)info.st_nlink,
               w_user, user,
               w_group, group,
               w_size, (long)info.st_size,
               timestr,
               entry->d_name);

        // 5. symbolic link: show where it points
        if (S_ISLNK(info.st_mode))
        {
            char target[1024];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }

    closedir(dp);
}

/* Convert st_mode number into a string like "drwxr-xr-x" */
void mode_to_string(mode_t mode, char str[])
{
    // file type
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    // owner
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    // group
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    // others
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    // special bits
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/* Count digits in a number, e.g. 16384 -> 5 (used for alignment) */
int num_len(long n)
{
    int len = 1;
    while (n >= 10)
    {
        n = n / 10;
        len++;
    }
    return len;
}
