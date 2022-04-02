# XPUManager Core Lib

## driver installation

follow install script

## header file setup
clone level-zero repo to some folder, and copy header files

```
$ git clone https://github.com/oneapi-src/level-zero.git
$ cd level-zero/include
$ ln -s `pwd` /usr/local/include/level_zero 
```

## library path setup
make sure shared lib can be found in required folder

```
sudo ln -s /usr/lib/x86_64-linux-gnu/libze_loader.so.1 \ 
    /usr/local/lib/libze_loader.so
```

## build

Call build_*.sh files, to build corresponding artifacts

