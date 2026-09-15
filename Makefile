# Compiler
CXX = g++

# Build with `make USE_GUI=1` to enable the SDL3 GUI.
USE_GUI ?= 0

# Flags
CXXFLAGS = -Ofast -march=native -mtune=native -flto -std=c++17
CPPFLAGS = -Iinclude

# Files
SRC = $(shell find src -name "*.cc")
GUI_SRC = $(shell find src/gui -name "*.cc") src/model/human.cc

ifeq ($(USE_GUI),1)
CPPFLAGS += -DUSE_GUI=1 $(shell pkg-config --cflags sdl3 sdl3-image)
LDLIBS += $(shell pkg-config --libs sdl3 sdl3-image)
BUILD_DIR = build/gui
else ifeq ($(USE_GUI),0)
SRC := $(filter-out $(GUI_SRC),$(SRC))
BUILD_DIR = build/headless
else
$(error USE_GUI must be either 0 or 1)
endif

OBJ = $(patsubst src/%.cc,$(BUILD_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

# Output
TARGET = app

TEST_TARGET = $(BUILD_DIR)/search_test
TEST_OBJ = $(BUILD_DIR)/tests/search_test.o

.PHONY: all clean test FORCE

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ) FORCE
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ) -o $@ $(LDLIBS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Wrap the clock so interruption tests can expire at a specific search node.
$(TEST_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJ)) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS) -Wl,--wrap=_ZNSt6chrono3_V212steady_clock3nowEv

$(TEST_OBJ): tests/search_test.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# `app` is shared by both configurations, so always relink it when make runs.
FORCE:

# Compile
$(BUILD_DIR)/%.o: src/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEP) $(TEST_OBJ:.o=.d)

# Clean
clean:
	rm -rf build $(TARGET)
