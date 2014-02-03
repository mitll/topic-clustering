BIN = ../../bin

SRC_DIR = ..

UTIL_DIR = $(SRC_DIR)/util

CLASSIFIER_DIR = $(SRC_DIR)/classifiers

STEMMER_DIR = $(SRC_DIR)/porter_stemmer

UTILS = $(UTIL_DIR)/basic_util.c \
	$(UTIL_DIR)/args_util.c \
	$(UTIL_DIR)/hash_util.c \
	$(CLASSIFIER_DIR)/classifier_util.c \
	$(STEMMER_DIR)/porter_stemmer.c

PROGS = $(BIN)/plsa_estimation_combined_file \
	$(BIN)/plsa_analysis 

CFLAGS = -O3 -Wall -static 

all : $(PROGS)
clean :
	rm -f $(PROGS)

$(BIN)/plsa_estimation_combined_file : plsa_estimation_combined_file.c clustering_util.c plsa.c ../classifiers/classifier_util.c
	gcc $(CFLAGS) -o $@ $< clustering_util.c plsa.c $(UTILS) -lm -I$(SRC_DIR)

$(BIN)/plsa_analysis : plsa_analysis.c clustering_util.c plsa.c
	gcc $(CFLAGS) -o $@ $< clustering_util.c plsa.c $(UTILS) -lm -I$(SRC_DIR)


