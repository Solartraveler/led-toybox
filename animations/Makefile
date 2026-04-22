#I love ChatGPT for generating this script

# Tool to use
TOOL = ../src/utilities/pixelAnimate/bin/PixelXml2ani

# Directories
SRC_DIR = .
BUILD_DIR = build

# Find all XML files
XML_FILES := $(wildcard $(SRC_DIR)/*.xml)

# Generate corresponding .ani output files in build/
ANI_FILES := $(patsubst $(SRC_DIR)/%.xml,$(BUILD_DIR)/%.ani,$(XML_FILES))

# Default target
all: $(BUILD_DIR) $(ANI_FILES)

# Rule to create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Pattern rule: convert .xml to .ani
$(BUILD_DIR)/%.ani: $(SRC_DIR)/%.xml
	$(TOOL) $< $@

# Clean target
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
