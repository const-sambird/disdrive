# DISDRIVE

A discord-based network block device that provides 51,200 bytes of storage for no cost

This is the worst thing i have ever made, or god willing will ever make, and the code quality reflects that

## Installation

you're going to need to install some things, though i confess i scarcely remember the process of building a sane development environment for this; at the very least, you will need a standard C development environment (gcc, etc) and node >= 18 installed for the discord bridge.

installing the node modules is easy: `npm install`

after this, you'll need the dependencies:

- nbdkit
- nbd (separate from nbdkit)
- libcurl
- mkfs.vfat

then it's a simple `make` to create `disdrive.so`; after that, run the driver. you'll need two separate shells for this, i'm so sorry.

shell 1:
```
node ./discordBridge.js
```
ensure this connects to discord before starting shell 2

shell 2:
```
sudo ./run.sh
```

## Inspiration

[the idea is based on this](http://tom7.org/harder/) and is legally a 'harder drive'
