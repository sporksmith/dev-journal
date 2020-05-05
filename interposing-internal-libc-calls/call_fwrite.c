#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *msg = "Hello fwrite\n";
    fwrite(msg, 1, strlen(msg), stdout);
    return 0;
}
