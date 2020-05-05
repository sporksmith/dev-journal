#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *msg = "Hello write\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    return 0;
}
