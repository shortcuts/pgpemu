.PHONY: build clean menuconfig flash monitor run install format test companion-build companion-install companion-reinstall companion-start companion-log companion-test companion-clean wiki-serve
.DEFAULT_GOAL := install-deps

IDF_EXPORT := . $(HOME)/esp/v5.4.1/esp-idf/export.sh >/dev/null
# Serial port for the built-in USB-Serial-JTAG on the ESP32-C3. Override on the
# command line if your device enumerates elsewhere, e.g. `make monitor PORT=/dev/cu.usbmodem1234`.
PORT ?= /dev/tty.usbmodem101

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

flash: build ## Flashes the firmware over the built-in USB-JTAG adapter (same method as the VSCode extension's "JTAG" flash method)
	$(IDF_EXPORT) && cd ./pgpemu-esp32 && openocd -f board/esp32c3-builtin.cfg -c "program_esp_bins build flasher_args.json verify reset exit"

monitor: ## Opens the serial monitor
	$(IDF_EXPORT) && cd ./pgpemu-esp32 && idf.py -p $(PORT) monitor

run: flash monitor ## Builds, flashes and monitors in one go

install: flash companion-install ## Flashes the firmware and installs the companion app in one go

##@ Code

format: ## Formats the firmware C sources
	cd ./pgpemu-esp32 && make -f Makefile.format format

test: ## Runs the PC unit test suite
	./run_tests.sh

##@ Companion app (Android)

APP_ID := com.pgpemu.companion

companion-build: ## Builds the companion app (debug APK)
	cd ./companion-app && ./gradlew assembleDebug

companion-install: ## Installs the companion app on a connected phone
	cd ./companion-app && ./gradlew installDebug

companion-reinstall: ## Uninstalls then reinstalls the companion app on a connected phone
	adb uninstall $(APP_ID) || true
	cd ./companion-app && ./gradlew installDebug

companion-start: ## Launches the companion app on a connected phone
	adb shell am start -n $(APP_ID)/.MainActivity

companion-log: ## Tails the companion app's logcat on a connected phone
	adb logcat --pid=$$(adb shell pidof $(APP_ID))

companion-test: ## Runs the companion app's unit tests
	cd ./companion-app && ./gradlew test

companion-clean: ## Removes the companion app's Gradle build output
	cd ./companion-app && ./gradlew clean

##@ Wiki

wiki-serve: ## Serves the docs wiki locally at http://localhost:8080
	cd ./docs/wiki && python3 -m http.server 8080
