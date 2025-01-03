.PHONY: all taichi clean

export CXX=g++
export CXXFLAGS=-fPIC -Wall -std=c++17
export LLVM_CXX_FLAGS=$(shell llvm-config --cxxflags | xargs)
export LLVM_LD_FLAGS=$(shell llvm-config --ldflags --libs core executionengine mcjit native | xargs)

all: taichi

taichi:
	$(MAKE) -C taichi

clean:
	$(MAKE) -C taichi clean
