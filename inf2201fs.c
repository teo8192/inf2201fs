#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

static struct options {
	const char *image;
	int version;
	int show_help;
} options;

#define OPTION(t, p) \
	{ t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--image=%s", image),
	OPTION("-i %s", image),
	OPTION("--version=%d", version),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

struct directory {
	int location;
	int size;
};

enum {
	SECTOR_SIZE = 512,
	MAX_NUM_FILES = (SECTOR_SIZE / sizeof(struct directory))
};

static struct directory file_dir[MAX_NUM_FILES];
static int num_files;

static void *fs_init(struct fuse_conn_info *conn,
					 struct fuse_config *cfg)
{
	int kernel_size;
	short buf[SECTOR_SIZE >> 1];
	(void) conn;
	FILE *image;

	cfg->kernel_cache = 0;

	num_files = 0;

	image = fopen(options.image, "rb");

	assert(image > 0);
	
	fread(buf, SECTOR_SIZE, 1, image); 
	kernel_size = buf[1];

	fseek(image, (kernel_size + 1) * SECTOR_SIZE, SEEK_SET);
	fread(file_dir, SECTOR_SIZE, 1, image);

	for (num_files = 0; num_files < MAX_NUM_FILES && (file_dir[num_files].location | file_dir[num_files].size) != 0; ++num_files);

	fclose(image);

	return NULL;
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;
	int file;

	for (int i = 1; i < strlen(path); ++i)
		if (path[i] > '9' || path[i] < '0')
			return -ENOENT;

	if (strlen(path) > 1)
		file = atoi(path+1);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (file < num_files && file >= 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = file_dir[file].size * SECTOR_SIZE;
	} else
		res = -ENOENT;

	return res;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi,
		enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	char filename[256];

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	for (int i = 0; i < num_files; ++i) {
		sprintf(filename, "%d", i);
		filler(buf, filename, NULL, 0, 0);
	}

	return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
	int file;

	for (int i = 1; i < strlen(path); ++i)
		if (path[i] > '9' || path[i] < '0')
			return -ENOENT;

	if (strlen(path) > 1)
		file = atoi(path+1);

	if (file < 0 || file >= num_files)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	int file;
	FILE *image;
	/*char tmp[32];*/

	for (int i = 1; i < strlen(path); ++i)
		if (path[i] > '9' || path[i] < '0')
			return -ENOENT;

	if (strlen(path) > 1)
		file = atoi(path+1);

	if (file < 0 || file >= num_files)
		return -ENOENT;

	/*sprintf(tmp, "%d %d\n", file_dir[file].location, file_dir[file].size);*/

	/*len = strlen(tmp);*/
	len = file_dir[file].size * SECTOR_SIZE;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;

		/*memcpy(buf, tmp + offset, size);*/
		/*size = strlen(buf);*/

		image = fopen(options.image, "rb");

		fseek(image, file_dir[file].location * SECTOR_SIZE + offset, SEEK_SET);
		fread(buf, size, 1, image);

		fclose(image);
	} else
		size = 0;

	return size;
}

static const struct fuse_operations fs_oper = {
	.init = fs_init,
	.getattr = fs_getattr,
	.readdir = fs_readdir,
	.open = fs_open,
	.read = fs_read
};

static void show_help(const char *progname)
{
	printf("This is the help you can get from %s.", progname);
}

int main(int argc, char *argv[])
{
	int ret;
	char cwd[PATH_MAX];
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return 1;

	sprintf(cwd, "%s/image", cwd);

	options.image = strdup(cwd);
	options.version = -1;

	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	ret = fuse_main(args.argc, args.argv, &fs_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
