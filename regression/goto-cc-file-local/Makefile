default: tests.log

include ../../src/config.inc
include ../../src/common

ifeq ($(BUILD_ENV_),MSVC)
	exe=../../../src/goto-cc/goto-cl
	is_windows=true
else
	exe=../../../src/goto-cc/goto-cc
	is_windows=false
endif

test:
	@../test.pl -e -p -c '../chain.sh $(exe) ../../../src/goto-instrument/goto-instrument ../../../src/cbmc/cbmc $(is_windows)'

tests.log:
	@../test.pl -e -p -c '../chain.sh $(exe) ../../../src/goto-instrument/goto-instrument ../../../src/cbmc/cbmc $(is_windows)'

clean:
	$(RM) tests.log
	@for dir in *; do \
		if [ -d "$$dir" ]; then \
			cd "$$dir"; \
			$(RM) *.out *.gb; \
			cd ..; \
		fi \
	done
