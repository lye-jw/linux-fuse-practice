#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

char directories[256][256];
int dir_idx = -1;

char files[256][256];
int file_idx = -1;

char file_contents[256][256];
int content_idx = -1;

static int prac_getattr(const char *path, struct stat *stbuf,
	struct fuse_file_info *fi);

static int prac_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

static int prac_read(const char *path, char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi);

static int prac_write(const char *path, const char *buffer, size_t size, off_t offset,
    struct fuse_file_info *fi);

static int prac_mkdir(const char *path, mode_t mode);
static int prac_mknod(const char *path, mode_t mode, dev_t rdev);
int is_dir(const char *path);
int is_file(const char *path);
int get_file_index(const char *path);

static const struct fuse_operations prac_ops = {
    .getattr = prac_getattr,
    .readdir = prac_readdir,
    .read = prac_read,
    .write = prac_write,
    .mkdir = prac_mkdir,
    .mknod = prac_mknod
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &prac_ops, NULL);
}

static int prac_getattr(const char *path, struct stat *stbuf,
	struct fuse_file_info *fi)
{
	stbuf->st_uid = getuid();	// Owner id
	stbuf->st_gid = getgid();	// Group id
	stbuf->st_atime = time( NULL ); // Last access time
	stbuf->st_mtime = time( NULL ); // Last modified time

	if (strcmp(path, "/") == 0 || is_dir(path) == 1) {
		// If root or any directory, set directory mode & permission bits
		// 2 hardlinks as directory has links "/." and "/.."
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (is_file(path) == 1) {
		// If file, set file mode & permission bits
		// Only link to itself
		// All file sizes 1024 bytes
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = 1024;
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
            filler(buffer, directories[i], NULL, 0, 0);
        }
        for (int j = 0; j <= file_idx; j++) {
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

int is_dir(const char *path)
{
    path++; // Keep name without "/"
    for (int i = 0; i <= dir_idx; i++) {
        if (strcmp(path, directories[i]) == 0) {
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
