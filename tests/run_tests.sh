#!/bin/bash
set -e

# Move to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

# Ensure compiler is built
echo "=== Building Void Compiler ==="
cmake --build build

echo ""
echo "=== Running Void Test Suite ==="

PASSED=0
FAILED=0

# Array of test files and their expected exit codes
declare -A EXPECTED_EXITS=(
    ["tests/01_while_basic.pn"]=10
    ["tests/02_while_break.pn"]=4
    ["tests/03_while_continue.pn"]=9
    ["tests/04_while_nested.pn"]=6
    ["tests/05_while_vectors.pn"]=55
    ["tests/06_vector_types.pn"]=0
    ["tests/07_vector_advanced.pn"]=10
)

# Sort test files
TEST_FILES=$(ls tests/*.pn | sort)

for TEST in $TEST_FILES; do
    TEST_NAME=$(basename "$TEST")
    EXPECTED_CODE=${EXPECTED_EXITS["$TEST"]:-0}
    
    echo -n "Running $TEST_NAME ... "
    
    # Compile
    if ! ./build/void "$TEST" > /dev/null 2>&1; then
        echo -e "\033[31m[COMPILE FAIL]\033[0m"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    # Run
    set +e
    ./out > /dev/null 2>&1
    ACTUAL_CODE=$?
    set -e
    
    if [ "$ACTUAL_CODE" -eq "$EXPECTED_CODE" ]; then
        echo -e "\033[32m[PASS]\033[0m (exit: $ACTUAL_CODE)"
        PASSED=$((PASSED + 1))
    else
        echo -e "\033[31m[FAIL]\033[0m (expected: $EXPECTED_CODE, got: $ACTUAL_CODE)"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "=== Test Summary ==="
echo "Total Tests: $((PASSED + FAILED))"
echo -e "Passed: \033[32m$PASSED\033[0m"
if [ "$FAILED" -gt 0 ]; then
    echo -e "Failed: \033[31m$FAILED\033[0m"
    exit 1
else
    echo "All tests passed successfully!"
    exit 0
fi
