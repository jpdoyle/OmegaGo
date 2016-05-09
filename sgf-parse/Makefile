default: build/Makefile

clean:
	rm -r build/

build/Makefile: CMakeLists.txt $(wildcard src/*.{cpp,hpp})
	mkdir -p build
	cd build; cmake .. && make; cd -
