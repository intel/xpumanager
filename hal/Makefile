DIRS = amc fwupd core
.PHONY: $(DIRS)

MAKE += --no-print-directory

all:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir; \
	done

debug:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir debug; \
	done

clean:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir clean; \
	done

distclean:
	@rm -rf output

