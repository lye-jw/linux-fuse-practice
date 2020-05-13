#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>

char directories[256][256];
int dir_idx = -1;

char files[256][256];
int file_idx = -1;

char file_contents[256][256];
int content_idx = -1;

static void *prac_init(struct fuse_conn_info *conn, struct fuse_config *cfg);

static int prac_getattr(const char *path, struct stat *stbuf,
    struct fuse_file_info *fi);

static int prac_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

static int prac_read(const char *path, char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi);

static int prac_truncate(const char *path, off_t size, struct fuse_file_info *fi);

static int prac_write(const char *path, const char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi);

static int prac_mkdir(const char *path, mode_t mode);
static int prac_mknod(const char *path, mode_t mode, dev_t rdev);
static int prac_rmdir(const char *path);
int is_dir(const char *path);
int is_file(const char *path);
int get_file_index(const char *path);
int get_dir_index(const char *path);
static int prac_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *outargs);

static struct options {
    const char *initfile_name;
    const char *initfile_content;
} options;

#define KEY_HELP 1

// t = option flag, a = attribute in options strucct
#define OPTION(t, a) { t, offsetof(struct options, a), 1 }
static const struct fuse_opt option_specs[] = {
    OPTION("--init=%s", initfile_name),
    OPTION("--content=%s", initfile_content),

    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_END
};

static const struct fuse_operations prac_ops = {
    .init = prac_init,
    .getattr = prac_getattr,
    .readdir = prac_readdir,
    .read = prac_read,
    .truncate = prac_truncate,
    .write = prac_write,
    .mkdir = prac_mkdir,
    .mknod = prac_mknod,
    .rmdir = prac_rmdir
};

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &options, option_specs, prac_opt_proc) == -1) {
        return 1;
    }

    /* Important: To make command line arg options work,
       pass fuse_args#argc and fuse_args#argv to fuse_main().
       Pass argc and argv only if no options or fuse_args needed. */
    return fuse_main(args.argc, args.argv, &prac_ops, NULL);
}

static void *prac_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    if (options.initfile_name != NULL) {
        file_idx++;
        strcpy(files[file_idx], options.initfile_name);
        if (options.initfile_content != NULL) {
            content_idx++;
            strcpy(file_contents[content_idx], options.initfile_content);
        }
    }
    return NULL;
}

static int prac_getattr(const char *path, struct stat *stbuf,
    struct fuse_file_info *fi)
{
    stbuf->st_uid = getuid();	// Owner id
    stbuf->st_gid = getgid();	// Group id
    stbuf->st_atime = time(NULL); // Last access time
    stbuf->st_mtime = time(NULL); // Last modified time

    if (strcmp(path, "/") == 0 || is_dir(path) == 1) {
        // If root or any directory, set directory mode & permission bits
        // 2 hardlinks as directory has links "/." and "/.."
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (is_file(path) == 1) {
        // If file, set file mode & permission bits
        // Only link to itself
        // Set size for all files
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        int index = get_file_index(path);
        stbuf->st_size = sizeof(file_contents[index]);
    } else {
        return -ENOENT;
    }

    return 0;
}

static int prac_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);
	
    if (strcmp(path, "/") == 0) {
        for (int i = 0; i <= dir_idx; i++) {
            if (strcmp(directories[i], "") != 0)
                filler(buffer, directories[i], NULL, 0, 0);
        }
        for (int j = 0; j <= file_idx; j++) {
            if (files[j] != NULL)
                filler(buffer, files[j], NULL, 0, 0);
        }
    }
    return 0;
}

static int prac_read(const char *path, char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
    int file_index = get_file_index(path);
    if (file_index >= 0) {
        // Read content of file starting from offset
        char *content = file_contents[file_index];
        memcpy(buffer, content + offset, size);
        size = strlen(content) - offset;
    } else {
        size = 0;
    }
    return size;
}

static int prac_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    if (size > 1024) {
        return -ENOMEM;
    }

    int index = get_file_index(path);
    int curr_content_size = sizeof(file_contents[index]);
    if (size > curr_content_size) {
        memset(file_contents[index] + curr_content_size, 0, size - curr_content_size);
    }

    return 0;
}

static int prac_write(const char *path, const char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
    int index = get_file_index(path);

    if (file_idx >= 0) {
        strcpy(file_contents[index] + offset, buffer);
        size = strlen(file_contents[index]) - offset;
    } else {
        size = 0;
    }
    return size;
}

static int prac_mkdir(const char *path, mode_t mode)
{
    path++;
    dir_idx++;
    strcpy(directories[dir_idx], path);
    return 0;
}

static int prac_mknod(const char *path, mode_t mode, dev_t rdev)
{
    path++;
    
    file_idx++;
    strcpy(files[file_idx], path);
	
    content_idx++;
    strcpy(file_contents[content_idx], "");

    return 0;
}

// TODO: Fix, only works once then causes crash
static int prac_rmdir(const char *path) {
    int dir_index = get_dir_index(path);
    strcpy(directories[dir_index], "");

    if (strcmp(directories[dir_index], "") != 0) {
        return -1;
    } else {
        return 0;
    }
}

int is_dir(const char *path)
{
    path++; // Keep name without "/"
    for (int i = 0; i <= dir_idx; i++) {
        if (strcmp(path, directories[i]) == 0 && strcmp(directories[i], "") != 0) {
            return 1;
        }
    }
    return 0;
}

int is_file(const char *path)
{
    path++;
    for (int i = 0; i <= file_idx; i++) {
        if (strcmp(path, files[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int get_file_index(const char *path)
{
    path++;
    for (int i = 0; i <= file_idx; i++) {
        if (strcmp(path, files[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int get_dir_index(const char *path)
{
    path++;
    for (int i = 0; i <= dir_idx; i++) {
        if (strcmp(path, directories[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int prac_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *outargs)
{
    if (key == KEY_HELP) {
        printf("usage: %s [options] <mountpoint>\n\n", outargs->argv[0]);
        printf("File-system specific options:\n"
       	    "    --init=<s>         Name of the init file\n"
       	    "    --content=<s>      Content of the init file\n"
       	    "\n");
	    // May not need to add to arg
	    fuse_opt_add_arg(outargs, "-h");
    }
    return 1;
}

