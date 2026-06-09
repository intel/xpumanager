project = "Intel XPU Manager"
# Intentional shadow of built-in — standard Sphinx idiom
copyright = "2026, Intel Corporation"  # noqa: A001
author = "Intel Corporation"
version = "2.0"
release = "2.0"
language = "en"

extensions = [
    "myst_parser",
]

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

html_theme = "sphinx_rtd_theme"
html_static_path = []
html_title = "Intel XPU Manager 2.0"

html_theme_options = {
    "navigation_depth": 4,
    "titles_only": False,
}
