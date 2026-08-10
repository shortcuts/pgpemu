.PHONY: build clean menuconfig monitor format test
.DEFAULT_GOAL := install-deps

IDF_EXPORT := . $(HOME)/esp/v5.4.1/esp-idf/export.sh >/dev/null

##@ Global

help: ## Prints help.
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Setup

install-deps: ## Install deps
	brew install cmake ninja dfu-util ccache clang-format
	git clone -b v5.4.1  --depth 1 --recursive https://github.com/espressif/esp-idf.git $HOME/esp/v5.4.1/esp-idf

setup-esp-idf: ## Setup the current session for esp-idf with target esp32c3
	./scripts/setup.fish

menuconfig: ## Opens the menuconfig
	$(IDF_EXPORT) && cd ./pgpemu-esp32 && idf.py menuconfig

build: ## Builds the firmware (no flash)
	$(IDF_EXPORT) && cd ./pgpemu-esp32 && idf.py build

clean: ## Removes the firmware build directory
	$(IDF_EXPORT) && cd ./pgpemu-esp32 && idf.py fullclean

monitor: ## Monitors the flash
	./scripts/monitor.fish

##@ Code

format: ## Formats the firmware C sources
	cd ./pgpemu-esp32 && make -f Makefile.format format

test: ## Runs the PC unit test suite
	./run_tests.sh
