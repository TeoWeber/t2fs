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
BIN_DIR=./bin
INC_DIR=./include
SRC_DIR=./src

all: $(BIN_DIR)/t2fs.o $(BIN_DIR)/support.o
	$(CC) $(BIN_DIR)/*.o $(LIB_DIR)/*.o -o tst

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $< -Wall
	mv $(@F) $(BIN_DIR)

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
