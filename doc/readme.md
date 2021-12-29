# Install dependencies and build doc

```
apt install doxygen liblua5.2-dev

pip install Sphinx sphinx_rtd_theme sphinxcontrib-openapi  apispec-webframeworks myst-parser prometheus-client

./build.sh
```

# Provisioning doc

XPU Manager doc will be generated into folder build/html in html format

To provision the doc, you can simply run

```
python3 -m http.server -d build/html
```

Then you can access the doc from browser.