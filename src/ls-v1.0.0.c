/*
* Programming Assignment 02: lsv1.6.0
* Features: -l (long listing), column display (down then across),
*           -x (horizontal display), alphabetical sort, colorized output,
*           -R (recursive listing)
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls -x
*       $ ./bin/ls -R
*       $ ./bin/ls -lR /etc
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
#include <sys/ioctl.h>

#define SIX_MONTHS (6L * 30 * 24 * 60 * 60)
#define SPACING 2

#define MODE_VERTICAL   0
#define MODE_LONG       1
#define MODE_HORIZONTAL 2

#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

extern int errno;

void do_ls(const char *dir, int mode, int recursive);   // CHANGED: recursive added
void print_vertical(const char *dir, char **names, int count, int maxlen);
void print_horizontal(const char *dir, char **names, int count, int maxlen);
void print_long(const char *dir, char **names, int count);
void get_user_name(uid_t uid, char buf[], int size);
void get_group_name(gid_t gid, char buf[], int size);
void mode_to_string(mode_t mode, char str[]);
int num_len(long n);
int get_terminal_width(void);
int compare_names(const void *a, const void *b);
int ends_with(const char *name, const char *ext);
const char *get_color(const char *dir, const char *name);
void print_colored(const char *dir, const char *name);
void print_spaces(int n);

int main(int argc, char *argv[])
{
    int opt;
    int mode = MODE_VERTICAL;
    int recursive_flag = 0;     // NEW

    while ((opt = getopt(argc, argv, "lxR")) != -1)   // NEW: "lxR"
    {
        if (opt == 'l')
            mode = MODE_LONG;
        else if (opt == 'x')
            mode = MODE_HORIZONTAL;
        else if (opt == 'R')
            recursive_flag = 1;
        else
        {
            fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory...]\n", argv[0]);
            exit(1);
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode, recursive_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            // with -R, do_ls prints its own "dir:" header
            if (!recursive_flag)
                printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode, recursive_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int mode, int recursive)
{
    struct dirent *entry;

    // NEW: with -R, print the directory name first (like real ls -R)
    if (recursive)
        printf("%s:\n", dir);

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    int capacity = 10;
    int count = 0;
    int maxlen = 0;

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

        names[count] = strdup(entry->d_name);
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

    qsort(names, count, sizeof(char *), compare_names);

    if (mode == MODE_LONG)
        print_long(dir, names, count);
    else if (mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_vertical(dir, names, count, maxlen);

    // NEW: go inside every sub-directory
    if (recursive)
    {
        for (int i = 0; i < count; i++)
        {
            // never go into "." or ".." (this would loop forever)
            if (strcmp(names[i], ".") == 0 || strcmp(names[i], "..") == 0)
                continue;

            // build the full path: parent_dir/name
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

            struct stat info;
            if (lstat(path, &info) == -1)
                continue;

            // lstat does not follow links, so a link to a folder is skipped
            if (S_ISDIR(info.st_mode))
            {
                printf("\n");
                do_ls(path, mode, recursive);    // recursive call
            }
        }
    }

    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

int compare_names(const void *a, const void *b)
{
    const char *name1 = *(const char **)a;
    const char *name2 = *(const char **)b;
    return strcmp(name1, name2);
}

int ends_with(const char *name, const char *ext)
{
    int name_len = strlen(name);
    int ext_len = strlen(ext);
    if (name_len < ext_len)
        return 0;
    return strcmp(name + name_len - ext_len, ext) == 0;
}

const char *get_color(const char *dir, const char *name)
{
    char path[1024];
    struct stat info;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &info) == -1)
        return "";

    mode_t mode = info.st_mode;

    if (S_ISLNK(mode))
        return COLOR_PINK;
    if (S_ISDIR(mode))
        return COLOR_BLUE;
    if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISSOCK(mode) || S_ISFIFO(mode))
        return COLOR_REVERSE;
    if (ends_with(name, ".tar") || ends_with(name, ".gz") || ends_with(name, ".zip"))
        return COLOR_RED;
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        return COLOR_GREEN;

    return "";
}

void print_colored(const char *dir, const char *name)
{
    printf("%s%s%s", get_color(dir, name), name, COLOR_RESET);
}

void print_spaces(int n)
{
    for (int i = 0; i < n; i++)
        putchar(' ');
}

int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80;
    return w.ws_col;
}

void print_vertical(const char *dir, char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + SPACING;

    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;

    int rows = (count + cols - 1) / cols;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int i = r + c * rows;
            if (i >= count)
                break;
            print_colored(dir, names[i]);
            print_spaces(col_width - strlen(names[i]));
        }
        printf("\n");
    }
}

void print_horizontal(const char *dir, char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + SPACING;
    int pos = 0;

    for (int i = 0; i < count; i++)
    {
        if (pos + col_width > term_width && pos > 0)
        {
            printf("\n");
            pos = 0;
        }
        print_colored(dir, names[i]);
        print_spaces(col_width - strlen(names[i]));
        pos = pos + col_width;
    }
    printf("\n");
}

void print_long(const char *dir, char **names, int count)
{
    struct stat info;
    char path[1024];
    char user[64], group[64];

    int w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    long total_blocks = 0;

    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &info) == -1)
            continue;

        total_blocks += info.st_blocks;

        int len = num_len(info.st_nlink);
        if (len > w_links) w_links = len;

        get_user_name(info.st_uid, user, sizeof(user));
        len = strlen(user);
        if (len > w_user) w_user = len;

        get_group_name(info.st_gid, group, sizeof(group));
        len = strlen(group);
        if (len > w_group) w_group = len;

        len = num_len(info.st_size);
        if (len > w_size) w_size = len;
    }

    printf("total %ld\n", total_blocks / 2);

    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &info) == -1)
        {
            perror(path);
            continue;
        }

        char perms[11];
        mode_to_string(info.st_mode, perms);

        get_user_name(info.st_uid, user, sizeof(user));
        get_group_name(info.st_gid, group, sizeof(group));

        char *t = ctime(&info.st_mtime);
        char timestr[20];
        time_t now = time(NULL);

        if (now - info.st_mtime > SIX_MONTHS || info.st_mtime > now)
            snprintf(timestr, sizeof(timestr), "%.6s  %.4s", t + 4, t + 20);
        else
            snprintf(timestr, sizeof(timestr), "%.12s", t + 4);

        printf("%s %*ld %-*s %-*s %*ld %s ",
               perms,
               w_links, (long)info.st_nlink,
               w_user, user,
               w_group, group,
               w_size, (long)info.st_size,
               timestr);
        print_colored(dir, names[i]);

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
}

void get_user_name(uid_t uid, char buf[], int size)
{
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        snprintf(buf, size, "%s", pw->pw_name);
    else
        snprintf(buf, size, "%d", uid);
}

void get_group_name(gid_t gid, char buf[], int size)
{
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        snprintf(buf, size, "%s", gr->gr_name);
    else
        snprintf(buf, size, "%d", gid);
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
