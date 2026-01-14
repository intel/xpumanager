# Copyright (C) 2025 Intel Corporation
# SPDX-License-Identifier: MIT

DIRS = oal hal ial
.PHONY: $(DIRS) release

MAKE += --no-print-directory

all:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir; \
	done

debug:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir debug; \
	done

release: all
# Create a release package for Debian
# Delete release folder if it exists
	@rm -rf release
# Create a new release folder
	@mkdir -p release/xpum
# Copy the tarball and necessary files
	@cp deployment/debian.tar.gz release/xpum
	@cp ial/cli/xpu-smi release/xpum/xpu-smi
	@cp hal/core/libxpum.so release/xpum/libxpum.so
# Extract the tarball
	@tar xzf release/xpum/debian.tar.gz -C release/xpum
# Build the package
	@cd release/xpum && dpkg-buildpackage -us -uc > /dev/null 2>&1
# Move the generated .deb files to the current directory
	@mv release/*.deb .
# Clean up the release folder
	@rm -rf release
# Create a new release folder
	@mkdir -p release
# Move the .deb files and necessary binaries to the release folder
	@mv *.deb release/
	@cp ial/cli/xpu-smi release/xpu-smi
	@cp hal/core/libxpum.so release/libxpum.so	

clean:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir clean; \
	done

distclean:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
# Clean up the release folder
	@rm -rf release
