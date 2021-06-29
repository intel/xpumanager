# XPUManager Core Lib

## build & copy file 

```
$ mkdir build # if build folder exists
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TEST=OFF
$ make 
$ make install # copy built binary to destination
```