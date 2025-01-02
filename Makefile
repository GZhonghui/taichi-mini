.PHONY: all taichi clean

export CXX = g++

export CXXFLAGS = -fPIC -Wall -std=c++17

all: taichi

taichi:
	$(MAKE) -C taichi

clean:
	$(MAKE) -C taichi clean
