.PHONY: all taichi debug clean

export CXX=g++
export CXXFLAGS=-fPIC -Wall -std=c++17
export LLVM_CXX_FLAGS=$(shell llvm-config --cxxflags | xargs)
export LLVM_LD_FLAGS=$(shell llvm-config --ldflags --libs core executionengine mcjit native | xargs)

all: taichi debug

taichi:
	$(MAKE) -C taichi

debug:
	$(CXX) $(LLVM_CXX_FLAGS) $(LLVM_LD_FLAGS) debug.cpp -o debug.out

clean:
	$(MAKE) -C taichi clean
