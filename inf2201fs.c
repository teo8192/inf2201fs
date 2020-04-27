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
	char name[0];
} __attribute__ ((packed));

#define DIR_SIZE(d) (sizeof(struct directory) + strlen(d->name) + 1)

enum {
	SECTOR_SIZE = 512,
	MAX_NUM_FILES = (SECTOR_SIZE / sizeof(struct directory))
};

static char file_dir_bytes[SECTOR_SIZE];
static struct directory *file_dir;
static int num_files;

#define FILE_AT(p) ((struct directory*)(&file_dir_bytes[p]))

static void update_dir(void)
{
	static int kernel_size = -1;
	short buf[SECTOR_SIZE >> 1];
	FILE *image;
	image = fopen(options.image, "rb");

	assert(image > 0);

	if (kernel_size < 0) {
		fread(buf, SECTOR_SIZE, 1, image); 
		kernel_size = buf[1];
	}

	fseek(image, (kernel_size + 1) * SECTOR_SIZE, SEEK_SET);
	fread(file_dir_bytes, SECTOR_SIZE, 1, image);

	fclose(image);
}

static void *fs_init(struct fuse_conn_info *conn,
					 struct fuse_config *cfg)
{
	(void) conn;
	int pos;

	cfg->kernel_cache = 0;

	num_files = 0;

	// TODO: better handeling
	update_dir();

	file_dir = file_dir_bytes;

	if (options.version < 0) {
		// figure out shit
		options.version = 1;
		for (char *c = &file_dir_bytes[sizeof(struct directory)]; *c != '\0'; ++c)
			if (*c <= 0x019 || *c >= 0x07f) {
				options.version = 0;
				break;
			}
	}

	for (num_files = 0, pos = 0; num_files < MAX_NUM_FILES && 
			(FILE_AT(pos)->location | FILE_AT(pos)->size) != 0; ++num_files)
		pos += options.version == 0 ? sizeof(struct directory) : DIR_SIZE(FILE_AT(pos));


	return NULL;
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;
	int file;

	update_dir();

	if (options.version == 0) {
		for (int i = 1; i < strlen(path); ++i)
			if (path[i] > '9' || path[i] < '0')
				return -ENOENT;

		if (strlen(path) > 1)
			file = atoi(path+1);
	} else
		res = -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	} else if (options.version > 0) {
		for (int pos = 0; (FILE_AT(pos)->location | FILE_AT(pos)->size) != 0; pos += DIR_SIZE(FILE_AT(pos))) {
			if (strcmp(FILE_AT(pos)->name, path + 1) == 0) {
				stbuf->st_mode = S_IFREG | 0444;
				stbuf->st_nlink = 1;
				stbuf->st_size = FILE_AT(pos)->size * SECTOR_SIZE;
				res = 0;
				break;
			}
		}
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

	update_dir();

	for (int i = 0, pos = 0; i < num_files; ++i) {
		if (options.version == 0) {
			sprintf(filename, "%d", i);
			filler(buf, filename, NULL, 0, 0);
		} else {
			filler(buf, FILE_AT(pos)->name, NULL, 0, 0);
			pos += DIR_SIZE(FILE_AT(pos));
		}
	}

	return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
	int file, ret;

	update_dir();

	// check if file exists
	if (options.version == 0) {
		for (int i = 1; i < strlen(path); ++i)
			if (path[i] > '9' || path[i] < '0')
				return -ENOENT;

		if (strlen(path) > 1)
			file = atoi(path+1);

		if (file < 0 || file >= num_files)
			return -ENOENT;
	} else {
		ret = -ENOENT;
		for (int i = 0, pos = 0; i < num_files; ++i) {
			if (strcmp(FILE_AT(pos)->name, path + 1) == 0) {
				ret = 0;
				break;
			}
			pos += DIR_SIZE(FILE_AT(pos));
		}
		if (ret != 0)
			return ret;
	}

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	int file = 0;
	FILE *image;
	struct directory *f;

	update_dir();

	if (options.version == 0) {
		for (int i = 1; i < strlen(path); ++i)
			if (path[i] > '9' || path[i] < '0')
				return -ENOENT;

		if (strlen(path) > 1)
			file = atoi(path+1);

		if (file < 0 || file >= num_files)
			return -ENOENT;
	} else {
		int ret = -ENOENT;
		for (int i = 0; i < num_files; ++i) {
			if (strcmp(FILE_AT(file)->name, path + 1) == 0) {
				ret = 0;
				break;
			}
			file += DIR_SIZE(FILE_AT(file));
		}
		if (ret != 0)
			return ret;
	}

	f = options.version == 0 ? &file_dir[file] : FILE_AT(file);

	len = f->size * SECTOR_SIZE;

	if (offset < len) {
		if (offset + size > len)
			size = len - offset;

		image = fopen(options.image, "rb");

		fseek(image, f->location * SECTOR_SIZE + offset, SEEK_SET);
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
	int ret, len;
	char cwd[PATH_MAX], def_file[] = "/image", name[] = "inf2201fs";
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return 1;

	len = strlen(cwd);
	for (int idx = 0; idx < strlen(def_file); ++idx)
		cwd[len++] = def_file[idx];

	options.image = strdup(cwd);
	options.version = -1;

	argv[0] = name;

	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	printf("Image: %s\n", options.image);

	ret = fuse_main(args.argc, args.argv, &fs_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
