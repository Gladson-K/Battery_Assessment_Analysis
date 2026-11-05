CC ?= gcc
CXX ?= g++
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -I../core -I../ml -I.
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -I../core -I../ml -I.
LDLIBS ?= -lm
BUILD ?= build

CORE_SOURCES := ../core/filters.c ../core/feature_builder.c ../core/battery_assessment.c \
                ../core/battery_profiles.c ../core/safety_controller.c ../core/telemetry.c
ML_SOURCES := ../ml/rf_infer.c
CORE_OBJECTS := $(patsubst ../core/%.c,$(BUILD)/%.o,$(CORE_SOURCES))
ML_OBJECTS := $(patsubst ../ml/%.c,$(BUILD)/%.o,$(ML_SOURCES))
CPP_OBJECTS := $(BUILD)/BatteryMonitor.o

.PHONY: all clean test

all: test_core test_model test_cpp

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: ../core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../ml/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/BatteryMonitor.o: ../core/BatteryMonitor.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_core: test_core.c $(CORE_OBJECTS)
	$(CC) $(CFLAGS) $< $(CORE_OBJECTS) -o $@ $(LDLIBS)

test_model: test_model.c $(CORE_OBJECTS) $(ML_OBJECTS)
	$(CC) $(CFLAGS) $< $(CORE_OBJECTS) $(ML_OBJECTS) -o $@ $(LDLIBS)

test_cpp: test_cpp.cpp $(CPP_OBJECTS) $(CORE_OBJECTS) $(ML_OBJECTS)
	$(CXX) $(CXXFLAGS) $< $(CPP_OBJECTS) $(CORE_OBJECTS) $(ML_OBJECTS) -o $@ $(LDLIBS)

test: all
	./test_core
	./test_model
	./test_cpp

clean:
	rm -rf $(BUILD) test_core test_model test_cpp
