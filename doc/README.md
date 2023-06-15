# Install dependencies and setup build environment

Before start build doc, you have to setup environment and denpendencies first.

First you need to install python3-venv, doxygen and liblua (for ubuntu you need install liblua5.2-dev; but for CentOS/RHEL/SUSE, you need install liblua5.3)

Then, you can run below commands to setup:

```
python3 -m venv {/path/to/venv}

source {/path/to/venv}/bin/activate

pip install --no-cache-dir -r requirements.txt
```

# Build doc

To build XPUM doc, run below command

```
./build.sh
```

# Provisioning doc

XPU Manager doc will be generated into folder build/html in html format

To provision the doc, you can simply run

```
python3 -m http.server -d build/html
```

Then you can access the doc from browser.