CC ?= gcc
CXX ?= g++
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -I../core -I../ml -I../drivers -I.
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -I../core -I../ml -I.
LDLIBS ?= -lm
BUILD ?= build

CORE_SOURCES := ../core/filters.c ../core/feature_builder.c ../core/battery_assessment.c \
                ../core/battery_profiles.c ../core/safety_controller.c ../core/telemetry.c
ML_SOURCES := ../ml/rf_infer.c
DRIVER_MATH_SOURCES := ../drivers/ina228_math.c
CORE_OBJECTS := $(patsubst ../core/%.c,$(BUILD)/%.o,$(CORE_SOURCES))
ML_OBJECTS := $(patsubst ../ml/%.c,$(BUILD)/%.o,$(ML_SOURCES))
DRIVER_MATH_OBJECTS := $(patsubst ../drivers/%.c,$(BUILD)/%.o,$(DRIVER_MATH_SOURCES))
CPP_OBJECTS := $(BUILD)/BatteryMonitor.o

.PHONY: all clean test

all: test_core test_model test_cpp test_ina228

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: ../core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../ml/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/BatteryMonitor.o: ../core/BatteryMonitor.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_core: test_core.c $(CORE_OBJECTS)
	$(CC) $(CFLAGS) $< $(CORE_OBJECTS) -o $@ $(LDLIBS)

test_model: test_model.c $(CORE_OBJECTS) $(ML_OBJECTS)
	$(CC) $(CFLAGS) $< $(CORE_OBJECTS) $(ML_OBJECTS) -o $@ $(LDLIBS)

test_cpp: test_cpp.cpp $(CPP_OBJECTS) $(CORE_OBJECTS) $(ML_OBJECTS)
	$(CXX) $(CXXFLAGS) $< $(CPP_OBJECTS) $(CORE_OBJECTS) $(ML_OBJECTS) -o $@ $(LDLIBS)

test_ina228: test_ina228.c $(DRIVER_MATH_OBJECTS)
	$(CC) $(CFLAGS) $< $(DRIVER_MATH_OBJECTS) -o $@ $(LDLIBS)

test: all
	./test_core
	./test_model
	./test_cpp
	./test_ina228

clean:
	rm -rf $(BUILD) test_core test_model test_cpp test_ina228
