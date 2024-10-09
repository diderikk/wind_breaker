# Makefile

# Define the source directory
SRC_DIR := src

# Find all .c and .h files in the source directory
SRC_FILES := $(shell find $(SRC_DIR) -type f \( -name "*.c" -o -name "*.h" \))

# Define the clang-format command
CLANG_FORMAT := clang-format -i

# Define the build directory
BUILD_DIR = build
EXECUTABLE = main

# Default target
all: build 	
	@./build/$(EXECUTABLE)

build: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)

# Format target
format:
	@echo "Formatting all C files in $(SRC_DIR)..."
	@$(foreach file, $(SRC_FILES), \
		echo "Formatting $(file)"; \
		$(CLANG_FORMAT) $(file);)

# Create the build directory and run cmake
$(BUILD_DIR)/Makefile: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..

# Clean target
clean:
	$(MAKE) -C $(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)

post:
	curl -X POST http://localhost:8080/ \
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)" \
  -H "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;" \
  -H "Accept-Language: en-US,en;q=0.5" \
  -H "Accept-Encoding: gzip, deflate, br" \
  -H "Connection: keep-alive" \
  -H "Content-Type: application/json" \
  -d '{"key1":"value1", "key2":"value2"}'

# Declare phony targets
.PHONY: all clean
