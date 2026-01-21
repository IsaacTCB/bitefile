TARGET := data.bite

ASSETS_DIR := assets
ASSETS := $(shell find $(ASSETS_DIR) -type f)

PY := python3
SCRIPT := wadlike.py

RM := rm -rf

.PHONY: clean

$(TARGET): $(SCRIPT) $(ASSETS)
	$(PY) $(SCRIPT) $(ASSETS) -o $(TARGET)

clean:
	$(RM) $(TARGET)
