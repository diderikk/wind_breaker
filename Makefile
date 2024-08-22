# Makefile

# Define the build directory
BUILD_DIR = build
EXECUTABLE = main

# Default target
all: build 	
	@./build/$(EXECUTABLE)

build: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)


# Create the build directory and run cmake
$(BUILD_DIR)/Makefile: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..

# Clean target
clean:
	$(MAKE) -C $(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)

# Declare phony targets
.PHONY: all clean
