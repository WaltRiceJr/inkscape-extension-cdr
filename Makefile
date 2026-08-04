# Builds the self-contained cdr2svg converter used by the Inkscape CDR
# import extension. Requires: g++ (C++17), pkg-config, and development
# headers for zlib, lcms2, icu (icu-i18n/icu-uc), and boost (headers only).

CXX      ?= g++
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Isrc/inc
CXXFLAGS += $(shell pkg-config --cflags zlib lcms2 icu-i18n icu-uc)
LIBS     := $(shell pkg-config --libs zlib lcms2 icu-i18n icu-uc)

BUILDDIR := build
SOURCES  := $(wildcard src/lib/cdr/*.cpp) $(wildcard src/lib/rvng/*.cpp) src/conv/cdr2svg.cpp
OBJECTS  := $(patsubst src/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

all: cdr2svg

cdr2svg: $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LIBS)

$(BUILDDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR) cdr2svg

.PHONY: all clean
