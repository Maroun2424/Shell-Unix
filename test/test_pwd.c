#include <stdio.h>
#include "commands.h" 

extern int pwd();

int main() {
    printf("Testing pwd function:\n");
    int result = pwd();
    printf("Return value: %d\n", result);
    return result;
}
