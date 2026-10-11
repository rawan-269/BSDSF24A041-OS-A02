/*
* Programming Assignment 02: lsv1.2.0
* Features: -l (long listing), column display (down then across)
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/ioctl.h>   // NEW: ioctl() for terminal width

#define SIX_MONTHS (6L * 30 * 24 * 60 * 60)
#define SPACING 2        // NEW: spaces between columns

extern int errno;

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char str[]);
int num_len(long n);
int get_terminal_width(void);                           // NEW
void print_vertical(char **names, int count, int maxlen); // NEW

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

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

    if (optind == argc)
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

/* NEW: read all names into an array, then print in columns */
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    int capacity = 10;   // how many names the array can hold right now
    int count = 0;       // how many names are actually stored
    int maxlen = 0;      // length of the longest name

    char **names = malloc(capacity * sizeof(char *));
    if (names == NULL)
    {
        perror("malloc failed");
        closedir(dp);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        // array is full: make it double size
        if (count == capacity)
        {
            capacity = capacity * 2;
            char **temp = realloc(names, capacity * sizeof(char *));
            if (temp == NULL)
            {
                perror("realloc failed");
                break;
            }
            names = temp;
        }

        names[count] = strdup(entry->d_name);   // make a copy of the name
        int len = strlen(entry->d_name);
        if (len > maxlen)
            maxlen = len;
        count++;
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }
    closedir(dp);

    print_vertical(names, count, maxlen);

    // free memory
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* NEW: get terminal width using ioctl, or 80 if it fails */
int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80;
    return w.ws_col;
}

/* NEW: print names "down then across" */
void print_vertical(char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + SPACING;

    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;     // at least one column, even for very long names

    int rows = (count + cols - 1) / cols;   // round up

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int i = r + c * rows;    // index of file at row r, column c
            if (i >= count)
                break;
            printf("%-*s", col_width, names[i]);
        }
        printf("\n");
    }
}

/* Long listing: like ls -l (same as v1.1.0) */
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

    int w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    long total_blocks = 0;

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

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

    printf("total %ld\n", total_blocks / 2);

    rewinddir(dp);

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

        char perms[11];
        mode_to_string(info.st_mode, perms);

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

        char *t = ctime(&info.st_mtime);
        char timestr[20];
        time_t now = time(NULL);

        if (now - info.st_mtime > SIX_MONTHS || info.st_mtime > now)
            snprintf(timestr, sizeof(timestr), "%.6s  %.4s", t + 4, t + 20);
        else
            snprintf(timestr, sizeof(timestr), "%.12s", t + 4);

        printf("%s %*ld %-*s %-*s %*ld %s %s",
               perms,
               w_links, (long)info.st_nlink,
               w_user, user,
               w_group, group,
               w_size, (long)info.st_size,
               timestr,
               entry->d_name);

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

void mode_to_string(mode_t mode, char str[])
{
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

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
