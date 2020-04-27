# inf2201fs

This is a file system implemented using FUSE to browse a specific type of image files used in the INF-2201 course at UiT.

## Dependencies

[libfuse](https://github.com/libfuse/libfuse) (version 3).

## Install

If you want to install it, run

```sh
make install
```

You might need to have superuser privileges.

## Usage

```sh
mount.inf2201fs mountpoint
```

The default image file is `image`.
You may specify another one with the `--image` flag (this is buggy).

The mountpoint needs to be a existing directory.

To unmount the filesystem, use either `fusermount -u [mountpoint]` or `umount [mountpoint]`.
It is recommended to use `fusermount -u`.
