# XPUManager Python Client

## Environment Set Up

### http proxy
if network requests is blocked, you can try to set up http proxy
```
$ export http_proxy="http://child-prc.intel.com:913"
$ export https_proxy="http://child-prc.intel.com:913"
```

### steps

1. install python and python-dev
```    
$ sudo apt install python3.8
$ sudo apt install python3-dev
```

2. install pip
```
$ sudo apt install python3-pip
```

3. upgrade pip
```
$ sudo python3 -m pip install -U pip \
    -i https://pypi.tuna.tsinghua.edu.cn/simple
```

4. install python dependencis
```
$ sudo python3 -m pip install -r requirements.txt \
    -i https://pypi.tuna.tsinghua.edu.cn/simple
```

## Run Python code

XPUManager consists of two parts:
- service
- CLI

### start service

Service should run in sudo mode
```
$ sudo DGM.py
```

### try CLI
```
$ cd client
$ dgmcli -l