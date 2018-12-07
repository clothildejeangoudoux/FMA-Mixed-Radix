# Compilation commands and flags
CC=gcc -I. -I../src -I../helper -I../gmp_reference -Wall
LIBS = -lm -lgmp

# Executable file
EXEC = fma_debug fma_tests

# Compilation rules
all: $(EXEC)

# Compiles the test program
fma_debug: fma_tests.c 
	$(CC) -g fma_tests.c ../src/fma_impl.c ../src/fma_empty.c ../helper/comparison-helper.c ../helper/decimal_helper.c ../gmp_reference/reference.c -o fma_debug -O0 $(LIBS)


fma_tests: fma_tests.o comparison-helper.o fma_impl.o decimal_helper.o reference.o fma_empty.o
	$(CC) -o fma_tests fma_tests.o comparison-helper.o fma_impl.o decimal_helper.o reference.o fma_empty.o -O0 $(LIBS)

fma_tests.o: fma_tests.c 
	$(CC) -o fma_tests.o -c fma_tests.c -g -O0 $(LIBS)

reference.o: ../gmp_reference/reference.c 
	$(CC) -o reference.o -c ../gmp_reference/reference.c -O3 -march=native $(LIBS)

decimal_helper.o: ../helper/decimal_helper.c 
	$(CC) -o decimal_helper.o -c ../helper/decimal_helper.c -O0 $(CFLAGS) $(LIBS)

comparison-helper.o: ../helper/comparison-helper.c 
	$(CC) -o comparison-helper.o -c ../helper/comparison-helper.c -O0 $(CFLAGS) $(LIBS)

fma_impl.o: ../src/fma_impl.c 
	$(CC) -o fma_impl.o -c ../src/fma_impl.c -O3 -march=native $(LIBS)

fma_empty.o: ../src/fma_empty.c 
	$(CC) -o fma_empty.o -c ../src/fma_empty.c -O3 -march=native $(LIBS)



# Remove .o files and executable
clean:
	rm -f *.o $(EXEC)

# Définition des cibles particulières
.PHONY: all, clean, mrproper
