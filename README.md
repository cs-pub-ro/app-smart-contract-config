# Proof-of-Concept of Configurable Smart Contract in C

This is a proof-of-concept smart contract program running on Unikraft.
It uses command line arguments, TCP networking and 9pfs filesystem mounting to configure the behavior of the smart contract.
It exposes a basic cryptographic hash functionality provided by [libsodium]().

## Setup

Apart from the current repository, you also need to clone:

* [Unikraft](https://github.com/unikraft/unikraft)
* [lwip](https://github.com/unikraft/lib-lwip)
* [newlib](https://github.com/unikraft/lib-newlib)
* [libsodium](https://github.com/unikraft/lib-libsodium)

Make sure each repository is on the `staging` branch.

It is easiest to use a file hierarchy such as:

```
.
|-- apps/
|   `-- app-smart-contract-config/
|-- libs/
|   |-- lwip/
|   |-- libsodium/
|   `-- newlib/
`-- unikraft/
```

If you use a file hierarhy different than the one above, then edit the `Makefile` file and update the variables to point to the correct location:
* Update the `UK_ROOT` variable to point to the location of the Unikraft clone.
* Update the `UK_LIBS` variable to point to the folder storing the library clones.

If you get a `Cannot find library` error at one of the following steps, you may need to add the `lib-` prefix to the libraries listed in the `LIBS` variable.
For instance, `$(UK_LIBS)/lwip` becomes `$(UK_LIBS)/lib-lwip`.

## Configure

Configure the build by running:

```
make menuconfig
```

The basic configuration is loaded from the `Config.uk` file.
The basic configuration is loaded from the `Config.uk` file.
Then add support for 9pfs, by selecting `Library Configuration` -> `vfscore: VFS Core Interface` -> `vfscore: VFS Configuration`.
The select `Automatically mount a root filesystem (/)` and select `9pfs`.
For the `Default root device` option fill `fs0`.
You should end up with an image such as the one in `9pfs_config.png`.
Save the conifugration and exit.

As a result of this, a `.config` file is created.
A KVM unikernel image will be built from the configuration.

## Build

Build the unikernel KVM image by running:

```
make
```

The resulting image is in the `build/` subfolder, usually named `build/app-smart-contract-config_kvm-x86_64`.

## Run

The resulting image is to be loaded as part of a KVM virtual machine with a software bridge allowing outside network access to the server application.
Start the image by using the `run` script (you require `sudo` privileges):

```
$ ./run
[...]
Booting from ROM...
1: Set IPv4 address 172.44.0.2 mask 255.255.255.0 gw 172.44.0.1
en1: Added
en1: Interface is up
Powered by
o.   .o       _ _               __ _
Oo   Oo  ___ (_) | __ __  __ _ ' _) :_
oO   oO ' _ `| | |/ /  _)' _` | |_|  _)
oOo oOO| | | | |   (| | | (_) |  _) :_
 OoOoO ._, ._:_:_,\_._,  .__,_:_, \___)
                    Dione 0.6.0~6a2069e
argument is 1024
Listening on port 1024...
```

The `1024` is passed as a command line argument.

The server now listens for "commands".
These commands are files in the `mnt/` folder that will be read and processed by the smart contract program.
Passing the commands `add`, `add2`, `sub`, `sub2`, `hash_generic`, `hash_sha256`, `hash_sha512`, `scalar_multiply`, `scalar_invert` will result in the program reading these files from the `mnt/` folder.

On another terminal, use `nc` to connect via TCP to the program and send the commands:

```
$ nc 172.44.0.2 1024
add
22
$ nc 172.44.0.2 1024
add2
575
$ nc 172.44.0.2 1024
sub
21
$ nc 172.44.0.2 1024
sub2
17109
$ nc 172.44.0.2 1024
hash_generic
ee9710661309687d26234b42086c3d74089fb99ec2c3250243bb82562b38f818
$ nc 172.44.0.2 1024
hash_sha256
ecdac3aca55681291802ec7bf75c10d0231d1c9925ea1af74232b73231eefc7d
$ nc 172.44.0.2 1024
hash_sha512
710bd17d77dd89bc11c37f68c0bb097d4a96cb7844365b857d13263d53829297d83e634c522fe11bd19b35667162a51d8eb8c6a1cb7e4672485915964bfd7ac6
$ nc 172.44.0.2 1024
scalar_multiply
bc7823c676c3b29920a856b0418c5d11f15c932503871140df96cafb3e75310a
$ nc 172.44.0.2 1024
scalar_invert
f1a031e616edc49a5801107ece1056574ffda72f5a11656ba8b9f466127d0e08
```

The running program will print out a summary of received commands:

```
Received connection from 172.44.0.1:48508
Received: add
Operation: addition
Sent: 22
Received connection from 172.44.0.1:48514
Received: add2
Operation: addition
Sent: 575
Received connection from 172.44.0.1:48516
Received: sub
Operation: subtraction
Sent: 21
Received connection from 172.44.0.1:48518
Received: sub2
Operation: subtraction
Sent: 17109
Received connection from 172.44.0.1:48532
Received: hash_generic
Operation: hash generic
Sent: ee9710661309687d26234b42086c3d74089fb99ec2c3250243bb82562b38f818
Received connection from 172.44.0.1:48534
Received: hash_sha256
Operation: hash sha256
Sent: ecdac3aca55681291802ec7bf75c10d0231d1c9925ea1af74232b73231eefc7d
Received connection from 172.44.0.1:48538
Received: hash_sha512
Operation: hash sha512
Sent: 710bd17d77dd89bc11c37f68c0bb097d4a96cb7844365b857d13263d53829297d83e634c522fe11bd19b35667162a51d8eb8c6a1cb7e4672485915964bfd7ac6
Received connection from 172.44.0.1:48542
Received: scalar_multiply
Operation: scalar multiply
Sent: bc7823c676c3b29920a856b0418c5d11f15c932503871140df96cafb3e75310a
Received connection from 172.44.0.1:48445
Received: scalar_invert
Operation: scalar invert
Sent: f1a031e616edc49a5801107ece1056574ffda72f5a11656ba8b9f466127d0e08
```

When done, close the server / KVM machine using `Ctrl+c`.
