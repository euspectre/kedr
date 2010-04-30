SAMPLE_NAME := mist_gen

MIST_INC_DIR ?= ./include
MIST_LIB_DIR ?= ./lib

CXXFLAGS := -Wall -Wextra -g -O0
LDFLAGS  := -Wl,-rpath,\$$ORIGIN/mist_engine/lib -L$(MIST_LIB_DIR) -lmist_engine

HEADERS := \
	Generator.h \
	Common.h \
	ValueLoader.h \
	TemplateLoader.h

OBJS := \
	Common.o \
	Generator.o \
	TemplateLoader.o \
	ValueLoader.o \
	main.o
	
.PHONY: all clean

all: $(SAMPLE_NAME)

$(SAMPLE_NAME): $(OBJS)
	g++ -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	g++ -c -o $@ $(CXXFLAGS) -I$(MIST_INC_DIR) $<
	
clean:
	rm -f $(SAMPLE_NAME) *.o
