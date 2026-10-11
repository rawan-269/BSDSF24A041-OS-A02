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

**Special bits:**

I also checked special bits in the same way:
- S_ISUID (setuid) shows 's' in place of the owner's 'x'
- S_ISGID (setgid) shows 's' in place of the group's 'x'
- S_ISVTX (sticky bit) shows 't' in place of the others' 'x'

If the special bit is on but execute is off, a capital 'S' or 'T' is shown.
