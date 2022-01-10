#!/bin/bash
PCM_HOME=pcm
if [ -n "$1" ]
then
    PCM_HOME=$1
fi
if [ ! -d $PCM_HOME ]
then
    echo "PCM_HOME $PCM_HOME doesn't exists!"
fi
cp $PCM_HOME/msr.* .
cp $PCM_HOME/cpucounters.* .
cp $PCM_HOME/pci.* .
cp $PCM_HOME/mmio.* .
cp $PCM_HOME/bw.* .
cp $PCM_HOME/utils.* .
cp $PCM_HOME/topology.* .
cp $PCM_HOME/debug.* .
cp $PCM_HOME/threadpool.* .
cp $PCM_HOME/resctrl.* .
cp $PCM_HOME/version.h .
cp $PCM_HOME/types.h .
cp $PCM_HOME/mutex.h .
cp $PCM_HOME/width_extender.h .
cp $PCM_HOME/lspci.h .
cp -r $PCM_HOME/exceptions .
make clean
make
