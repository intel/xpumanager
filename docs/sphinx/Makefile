SPHINXBUILD  ?= sphinx-build
SOURCEDIR     = .
BUILDDIR      = _build/html

.PHONY: help build clean

help:
	@echo "Usage:"
	@echo "  make build  build 2.0 docs"
	@echo "  make clean  remove build output"

build:
	$(SPHINXBUILD) -b html \
		-c $(SOURCEDIR) \
		$(SOURCEDIR) \
		$(BUILDDIR)/2.0

clean:
	rm -rf $(BUILDDIR)
