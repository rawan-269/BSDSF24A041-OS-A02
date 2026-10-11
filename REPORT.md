
# Operating Systems - Programming Assignment 02
## Report

---

## Feature 2: ls-v1.1.0 - Long Listing Format

### Q1. What is the crucial difference between the stat() and lstat() system calls? In the context of the ls command, when is it more appropriate to use lstat()?

Both stat() and lstat() are used to get information about a file, like its size, permissions, owner, and modification time. They store this information in a `struct stat`.

The main difference is how they handle **symbolic links**:

- **stat()** follows the symbolic link. So if I call stat() on a link, it gives me the information of the file that the link is pointing to (the target file).
- **lstat()** does not follow the link. It gives me the information about the link itself.

For normal files and directories, both of them give the same result.

In the ls command, it is better to use lstat() because ls -l needs to show the link as a link. For example:

    lrwxrwxrwx 1 user user 8 Oct 10 22:00 mylink -> Makefile

If I used stat() here, my program would show the details of Makefile instead, and the user would not know that mylink is a link. With lstat(), I can use the S_ISLNK() macro to detect the link, show 'l' as the file type, and then use readlink() to print where it points (-> Makefile).

Also, if a link is broken (its target file is deleted), stat() will fail, but lstat() will still work and show the link.

---

### Q2. The st_mode field in struct stat is an integer that contains both the file type and the permission bits. Explain how you can use bitwise operators (like &) and predefined macros (like S_IFDIR or S_IRUSR) to extract this information.

st_mode is just a number, but every bit inside it has a special meaning. Some bits tell the file type and the other bits tell the permissions. To check a specific bit, we use the bitwise AND operator (&) with a predefined macro (called a mask).

**Checking permissions:**

Each permission has its own macro. For example S_IRUSR means "owner can read". If I do:

    if (mode & S_IRUSR)

The & operator keeps only that one bit. If the result is non-zero, the bit is on, which means the owner has read permission. If the result is 0, the permission is not given.

In my code I used this to build the permission string:

    str[1] = (mode & S_IRUSR) ? 'r' : '-';   // owner read
    str[2] = (mode & S_IWUSR) ? 'w' : '-';   // owner write
    str[3] = (mode & S_IXUSR) ? 'x' : '-';   // owner execute

Same for group (S_IRGRP, S_IWGRP, S_IXGRP) and others (S_IROTH, S_IWOTH, S_IXOTH).

**Checking file type:**

The file type uses more than one bit, so we first take out all the type bits using the S_IFMT mask, and then compare:

    if ((mode & S_IFMT) == S_IFDIR)   // it is a directory

There are also easy macros which do the same work, like S_ISDIR(mode), S_ISLNK(mode), S_ISREG(mode). I used these in my code to set the first character ('d', 'l', '-', etc).

---

## Feature 3: ls-v1.2.0 - Column Display (Down Then Across)

### Q1. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient for this task?

In "down then across" format, the files are filled from top to bottom in the first column, and when the column is full, they continue in the next column. This is how the default ls command shows files.

My logic works in these steps:

1. First, I read all the filenames into a dynamic array and also find the length of the longest filename.
2. Then I find the terminal width using ioctl().
3. Column width = longest name + 2 spaces.
4. Number of columns = terminal width / column width.
5. Number of rows = (total files + columns - 1) / columns. This formula rounds up the answer, so no file is left out.
6. Then I use two loops. The outer loop goes through each row, and the inner loop goes through each column. For row r and column c, the index of the file is:

        index = r + c * rows

For example, if there are 10 files and 3 columns, then rows = 4, and the files are placed like this:

    0   4   8
    1   5   9
    2   6
    3   7

So the first row prints files 0, 4 and 8.

A simple single loop (0, 1, 2, 3 ...) is not enough because the terminal can only print line by line, from left to right and top to bottom. We cannot go back up to print in the previous column. If I use a single loop, the files will be printed in order across the line (0, 1, 2 in the first row), which is the horizontal format and not "down then across". So I have to calculate which file belongs in each position of the row, and that is why I need the total number of rows before I start printing.

---

### Q2. What is the purpose of the ioctl system call in this context? What would be the limitations of your program if you only used a fixed-width fallback (e.g., 80 columns) instead of detecting the terminal size?

ioctl() is a system call that is used to send special requests to a device. Here I used it with the TIOCGWINSZ request on the standard output to ask the terminal for its current size. It fills a struct winsize, and the ws_col field tells me how many characters fit in one line of the terminal.

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int width = w.ws_col;

I use this width to decide how many columns can fit on the screen. If ioctl() fails (for example when the output is sent to a file and not to a terminal), my program uses 80 as the default width.

If I only used a fixed width of 80, there would be these problems:

- If the terminal is wider than 80 (like a full screen window), a lot of space on the right will be empty, and the program will print more rows than needed. The output will not use the screen properly.
- If the terminal is smaller than 80, the lines will be longer than the screen, so they will wrap to the next line and the columns will get mixed up and become hard to read.
- When the user resizes the window, the output will not adjust to the new size.

So detecting the real terminal size makes the output always fit the screen, just like the real ls command.

**Special bits:**

I also checked special bits in the same way:
- S_ISUID (setuid) shows 's' in place of the owner's 'x'
- S_ISGID (setgid) shows 's' in place of the group's 'x'
- S_ISVTX (sticky bit) shows 't' in place of the others' 'x'

If the special bit is on but execute is off, a capital 'S' or 'T' is shown.
