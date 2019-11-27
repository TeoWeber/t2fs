#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc -c
CFLAGS= -lm -Wall -Wextra -O2 -Wunreachable-code -Wuninitialized -Winit-self -std=gnu99
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

LIB=$(LIB_DIR)/libt2fs.a

all: $(BIN_DIR)/t2fs.o $(BIN_DIR)/support.o
	ar -crs $(LIB) $^ $(LIB_DIR)/apidisk.o $(LIB_DIR)/bitmap2.o 

$(BIN_DIR)/t2fs.o: $(SRC_DIR)/t2fs.c 
	$(CC) -o $@ $< -I$(INC_DIR) $(CFLAGS)

$(BIN_DIR)/support.o: $(SRC_DIR)/support.c 
	$(CC) -o $@ $< -I$(INC_DIR) $(CFLAGS)

tar: clean
	@cd .. && tar -zcvf t2fs.tar.gz T2FS

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~