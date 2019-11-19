#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: t2fs.o support.o
	ar -crs $(LIB_DIR)/libt2fs.a $^ $(BIN_DIR)/*.o

t2fs.o: 
	$(CC) -c $(SRC_DIR)/t2fs.c -o $(BIN_DIR)/$@ -Wall -Wextra -02

support.o:
	$(CC) -c $(SRC_DIR)/support.c -o $(BIN_DIR)/$@ -Wall -Wextra -02

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/t2fs.o $(BIN_DIR)/support.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~


