.PHONY: all clean

all: llvm_taichi.so

llvm_taichi.so: llvm_export.o llvm_manager.o
	$(CXX) $(LLVM_LD_FLAGS) -shared llvm_export.o llvm_manager.o -o llvm_taichi.so

llvm_export.o: llvm_export.cpp llvm_export.h
	$(CXX) $(CXXFLAGS) $(LLVM_CXX_FLAGS) -c llvm_export.cpp -o llvm_export.o

llvm_manager.o: llvm_manager.cpp llvm_manager.h
	$(CXX) $(CXXFLAGS) $(LLVM_CXX_FLAGS) -c llvm_manager.cpp -o llvm_manager.o

clean:
	rm -rf *.o
	rm -rf *.so
