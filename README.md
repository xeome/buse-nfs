# buse-nfs

## Description

Project to demonstrate the use of the BUSE (Block Device in User Space) to create a block device that can be mounted as a filesystem. Callbacks can be used to implement the read and write operations of the block device and pretty much do anything you want with it. In this project, the block device is created and the data is synchronized periodically with a remote buffer, remote buffer represents a storage device in a remote server that can be accessed through the network.

## Building

To build the project, simply run the following command:

```bash
git clone https://github.com/xeome/buse-nfs.git
cd buse-nfs
cmake -B build
cmake --build build
```

## Usage

To use the project, you must first load the nbd kernel module. You can do this by running the following command:

```bash
sudo modprobe nbd
```

After that, you can run the `buse_nfs` binary. To see the available options, run the following command:

```bash
./build/buse_nfs --help
```

You can run the binary with no arguments to create a block device with the default options.

```bash
sudo ./build/buse_nfs
```

## Testing

To test the project, you can run the following command:

```bash
sudo ./chk.sh /dev/nbd0
```

## License

This project is licensed under the [GPL3 License](LICENSE).
