# Install dependencies and build doc

```
apt install doxygen liblua5.2-dev

python3 -m venv {/path/to/venv}

source {/path/to/venv}/bin/activate

pip install --no-cache-dir -r requirements.txt

./build.sh
```

# Provisioning doc

XPU Manager doc will be generated into folder build/html in html format

To provision the doc, you can simply run

```
python3 -m http.server -d build/html
```

Then you can access the doc from browser.