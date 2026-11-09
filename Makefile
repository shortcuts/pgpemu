.PHONY: build clean menuconfig flash monitor run install format test smoke-test smoke-test-selftest companion-build companion-build-release companion-install-release companion-install companion-reinstall companion-start companion-log companion-run companion-test companion-coverage companion-coverage-open companion-lint companion-format companion-clean wiki-serve screenshot
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
	$(MAKE) smoke-test-selftest

##@ Hardware smoke test

smoke-test: ## Maintainer only: pairing, reconnect and Control Service check on a flashed device (ARGS="--name <name>")
	python3 scripts/smoke_test.py $(ARGS)

smoke-test-selftest: ## Runs the smoke test's hardware-free self-test
	python3 -m unittest discover -s scripts -p 'test_*.py'

##@ Companion app (Android)

APP_ID := com.pgpemu.companion
# Gradle runs on the JVM that starts gradlew, and AGP 9.1.1 cannot compile on JDK 26.
JAVA_HOME ?= $(shell mise where java@17.0.2 2>/dev/null)
# The lint test harness reads these and ignores local.properties, so a stale path breaks it.
COMPANION_SDK := $(shell sed -n 's/^sdk.dir=//p' companion-app/local.properties 2>/dev/null)
GRADLEW := env $(foreach v,ANDROID_HOME ANDROID_SDK_ROOT,$(if $(wildcard $($(v))/.),,$(if $(wildcard $(COMPANION_SDK)/.),$(v)=$(COMPANION_SDK),-u $(v)))) JAVA_HOME=$(JAVA_HOME) ./gradlew

companion-build: ## Builds the companion app (debug APK)
	cd ./companion-app && $(GRADLEW) assembleDebug

companion-build-release: ## Builds the companion app (R8-minified release APK, unsigned)
	cd ./companion-app && $(GRADLEW) :app:assembleRelease

companion-install-release: ## Installs the R8-minified release APK, signed with the local debug key, on a connected phone
	cd ./companion-app && KEYSTORE_PATH=$(HOME)/.android/debug.keystore STORE_PASSWORD=android KEY_ALIAS=androiddebugkey KEY_PASSWORD=android $(GRADLEW) :app:assembleRelease
	adb install -r companion-app/app/build/outputs/apk/release/app-release.apk

companion-install: ## Installs the companion app on a connected phone
	cd ./companion-app && $(GRADLEW) installDebug

companion-reinstall: ## Uninstalls then reinstalls the companion app on a connected phone
	adb uninstall $(APP_ID) || true
	cd ./companion-app && $(GRADLEW) installDebug

companion-start: ## Launches the companion app on a connected phone
	adb shell am start -n $(APP_ID)/.MainActivity

companion-log: ## Starts the companion app if needed, then tails its logcat on a connected phone
	p() { adb shell pidof $(APP_ID) | tr -d '\r' | awk '{print $$1}'; }; \
	adb get-state >/dev/null || exit 1; \
	pid=$$(p); \
	[ -n "$$pid" ] || adb shell am start -n $(APP_ID)/.MainActivity; \
	i=0; while [ -z "$$pid" ] && [ $$i -lt 30 ]; do sleep 1; i=$$((i+1)); pid=$$(p); done; \
	[ -n "$$pid" ] || { echo "$(APP_ID) did not start within 30 s" >&2; exit 1; }; \
	adb logcat --pid=$$pid

companion-run: companion-install companion-start companion-log ## Installs, starts and tails the companion app on a connected phone

companion-test: ## Runs the companion app's unit tests
	cd ./companion-app && $(GRADLEW) test

companion-coverage: ## Runs companion unit tests and writes Kover HTML/XML reports
	cd ./companion-app && $(GRADLEW) koverHtmlReport koverXmlReport

companion-coverage-open: ## Opens the companion Kover HTML report
	open companion-app/build/reports/kover/html/index.html

companion-lint: ## Runs Android lint (lintRelease) on the companion app
	cd ./companion-app && $(GRADLEW) :app:lintRelease

companion-format: ## Formats the companion app's Kotlin sources
	cd ./companion-app && $(GRADLEW) ktlintFormat

companion-clean: ## Removes the companion app's Gradle build output
	cd ./companion-app && $(GRADLEW) clean

##@ Wiki

wiki-serve: ## Serves the docs wiki locally at http://localhost:8080
	cd ./docs/wiki && python3 -m http.server 8080

screenshot: ## Captures the companion app tabs from the attached phone for the wiki
	python3 scripts/screenshot_wiki.py
