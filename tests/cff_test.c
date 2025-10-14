// Test case for Control Flow Flattening pass
// Contains nested conditionals and loops to verify flattening effectiveness

#include <stdio.h>

int complex_flow(int a, int b) {
    int result = 0;
    
    // Nested conditionals
    if (a > 0) {
        if (b > 0) {
            result = a + b;
        } else {
            result = a - b;
        }
    } else {
        if (b > 0) {
            result = -a + b;
        } else {
            result = -a - b;
        }
    }

    // Loop with early exit
    for (int i = 0; i < a && i < 10; i++) {
        if (result > 100) {
            break;
        }
        result += b;
    }

    return result;
}

int main(int argc, char *argv[]) {
    printf("Result 1: %d\n", complex_flow(5, 3));   // Should print positive path
    printf("Result 2: %d\n", complex_flow(-5, -3)); // Should print negative path
    printf("Result 3: %d\n", complex_flow(15, 10)); // Should trigger early exit
    return 0;
}