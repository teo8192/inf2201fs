# inf2201fs

This is a file system implemented using FUSE to browse a specific type of image files used in the INF-2201 course at UiT.

## Dependecies

[libfuse](https://github.com/libfuse/libfuse) (version 3).

## Usage

```sh
mkdir mnt && make mnt
cd mnt
```

This is assuming you are in a directory with the image file present.
Default image file name is `image`.

When the binary is built, you can also mount with `./inf2201fs [--image=[imagename]] mountpoint`.
It may be unmounted with `fusermount -u [mountpoint]`.
