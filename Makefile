.PHONY: build
.DEFAULT_GOAL := install-deps

##@ Global

help: ## Prints help.
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Setup

install-deps: ## Install deps
	brew install cmake ninja dfu-util ccache
	git clone -b v5.4.1  --depth 1 --recursive https://github.com/espressif/esp-idf.git $HOME/esp/v5.4.1/esp-idf

setup-esp-idf: ## Setup the current session for esp-idf with target esp32c3
	./scripts/setup.fish

menuconfig: ## Opens the menuconfig
	cd ./pgpemu-esp32 && idf.py menuconfig

clean:
	cd ./pgpemu-esp32 && idf.py fullclean

monitor: ## Monitors the flash
	./scripts/monitor.fish

install-secrets:
	cd ./secrets && \
		poetry install --no-root && \
		poetry run ./secrets_upload.py secrets.yaml /dev/tty.usbmodem101

##@ Code

format:
	cd ./pgpemu-esp32 && make -f Makefile.format format
