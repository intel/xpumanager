# Prerequisites

Install prerequisites packages

```bash
sudo apt install -y python3 python3-venv python3-pip
```

Then run install script

```bash
./install.sh
```

In some case you may need to set up http proxies before run install.sh
```bash
export http_proxy=...
export https_proxy=...
```

# Daemon

Start daemon

```bash
sudo ./start.sh
```

Check daemon status and restart daemon if needed

```bash
sudo ./xpum-engine status

sudo ./xpum-engine restart xpum
```

Stop daemon

```bash
sudo ./stop.sh
```

# CLI

Show help

```bash
./xpumcli -h
```

List devices

```bash
./xpumcli discovery -l
```