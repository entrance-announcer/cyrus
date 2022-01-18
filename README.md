# cyrus

Command-line tool to write audio files to block devices for use with miley.

## Capabilities

Does not:

- resample (maybe with libresample, tho)

## Compatibility

This project targets Linux only.\
\
The POSIX compliant `stat` and `ioctl` functionality is used to validate the provided block device's features. These
would permit usage on all POSIX compliant operating systems. However, in the fairly
low-level <a href="https://en.wikipedia. org/wiki/Ioctl#:~:text=In%20computing%2C%20ioctl%20(an%20abbreviation, completely%20on%20the%20request%20code.">
ioctl</a> command, the `BLKGETSIZE64` value from `<linux/fs.h>` is used to read the root block device's size. Since this
program operates directly on the block device, a higher-level system call cannot be used. The usage of linux's
implementation of this device-specific call is what restricts compatibility to linux, only.