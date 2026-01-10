#!/bin/bash

# Test runner script for PGPemu PC-based unit tests
# Compiles and runs all tests in pgpemu-esp32/main/pc/ directory
# Usage: ./run_tests.sh [test_name]
#   - Without args: runs all tests
#   - With arg: runs specific test (e.g., ./run_tests.sh test_config_storage)

set -e

TEST_DIR="pgpemu-esp32/main/pc"
FAILED_TESTS=()
PASSED_TESTS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
	echo -e "\n${BLUE}========================================${NC}"
	echo -e "${BLUE}$1${NC}"
	echo -e "${BLUE}========================================${NC}\n"
}

print_success() {
	echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
	echo -e "${RED}✗ $1${NC}"
}

print_warning() {
	echo -e "${YELLOW}⚠ $1${NC}"
}

# Verify test directory exists
if [ ! -d "$TEST_DIR" ]; then
	print_error "Test directory not found: $TEST_DIR"
	exit 1
fi

print_header "PGPemu PC Unit Test Runner"

# Get list of test files
if [ -z "$1" ]; then
	# Run all tests except those with ESP dependencies
	TEST_FILES=$(ls "$TEST_DIR"/test_*.c 2>/dev/null | grep -v "test_nvs_helper" || echo "")
else
	# Run specific test
	TEST_FILES="$TEST_DIR/test_$1.c"
fi

if [ -z "$TEST_FILES" ]; then
	print_error "No test files found in $TEST_DIR"
	exit 1
fi

echo "Found tests:"
for test_file in $TEST_FILES; do
	test_name=$(basename "$test_file" .c)
	echo "  - $test_name"
done
echo ""

# Run each test
test_count=0
for test_file in $TEST_FILES; do
	test_name=$(basename "$test_file" .c)
	test_count=$((test_count + 1))

	print_header "Running: $test_name"

	# Compile
	compile_output=$(cd "$TEST_DIR" && gcc -o "$test_name" "$test_name.c" -std=c99 2>&1)
	compile_status=$?

	if [ $compile_status -ne 0 ]; then
		print_error "Compilation failed for $test_name"
		echo "$compile_output"
		FAILED_TESTS+=("$test_name (compilation)")
		continue
	fi
	print_success "Compiled: $test_name"

	# Run
	test_output=$("$TEST_DIR/$test_name" 2>&1)
	test_status=$?

	if [ $test_status -eq 0 ]; then
		# Show test output
		echo "$test_output"
		print_success "Passed: $test_name"
		PASSED_TESTS+=("$test_name")
	else
		print_error "Test failed: $test_name"
		echo "$test_output"
		FAILED_TESTS+=("$test_name")
	fi
done

# Summary
print_header "Test Summary"

echo "Total tests: $test_count"
echo "Passed: ${#PASSED_TESTS[@]}"
echo "Failed: ${#FAILED_TESTS[@]}"

if [ ${#PASSED_TESTS[@]} -gt 0 ]; then
	echo -e "\n${GREEN}Passed tests:${NC}"
	for test in "${PASSED_TESTS[@]}"; do
		echo -e "  ${GREEN}✓${NC} $test"
	done
fi

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
	echo -e "\n${RED}Failed tests:${NC}"
	for test in "${FAILED_TESTS[@]}"; do
		echo -e "  ${RED}✗${NC} $test"
	done
	exit 1
else
	echo -e "\n${GREEN}All tests passed!${NC}\n"
	exit 0
fi
