# Shortcuts over CMake (CMakePresets.json): every target that runs something
# builds first, so nothing runs a stale binary.
#
#   make                  configure (the first time) and build everything
#   make test-clase1      the tests of clase 1 (... test-clase5)
#   make test-given       the tests of the given code
#   make qualifying       the qualifying
#   make race             the race; options with ARGS, e.g. make race ARGS="--red-flag 5"
#   make tsan             the race on the stress track, built with ThreadSanitizer
#   make gui              the GUI, at http://localhost:8000
#   make clean            delete build/

.PHONY: all test-given qualifying race tsan gui clean

all:
	@test -f build/CMakeCache.txt || cmake --preset default
	cmake --build --preset default

# Not .PHONY: make skips pattern rules for phony targets.
test-clase%: all
	build/test_clase$*

test-given: all
	build/test_given

qualifying: all
	build/qualifying $(ARGS)

race: all
	build/race $(ARGS)

# ThreadSanitizer: reports every data race it sees (exit 66). Its own build
# directory (build-tsan/), so it never mixes with the normal build.
tsan:
	@test -f build-tsan/CMakeCache.txt || cmake --preset tsan
	cmake --build --preset tsan --target race
	build-tsan/race tracks/stress.track

gui:
	python3 gui/serve.py

clean:
	rm -rf build build-tsan
