#--------------------------------------------------
# Makefile final do trabalho
#--------------------------------------------------

# --- Variaveis de Configuracao ---

# Compilador a ser utilizado
CC = gcc

# Flags para compilador
CFLAGS = -Wall -g

# Comando para remover arquivos
RM = rm -rf

# --- Definicao dos arquivos ---

# Nome do executavel final
TARGET = gbv

# Arquivos fonte (.c)
SRCS = main.c gbv.c util.c

# Gera a lista de arquivos (.o) automaticamente a partir da lista de fontes
OBJS = $(SRCS:.c=.o)

# --- Regras de Construcao ---

# Alvo padrao
all: $(TARGET)

# Linka os arquivos objeto e cria o executavel final
$(TARGET): $(OBJS)
		$(CC) $(CFLAGS) -o $@ $^

# Regra padrao para compilar arquivos .c em .o
%.o: %.c
		$(CC) $(CFLAGS) -c -o $@ $<

# --- Dependencias Explicitas dos Cabecalhos ---

main.o: main.c gbv.h
gbv.o: gbv.c gbv.h util.h
util.o: util.c util.h

# --- Regras de limpeza dos arquivos gerados pela compilacao ---
clean:
	$(RM) $(OBJS) $(TARGET)

