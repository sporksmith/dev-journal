## Poking around at Posix message queues in Linux

I was recently reviewing some code introduced usage of posix message queues into some tests. I hadn't used them before, and nothing in the code base already used them, so I needed to familiarize myself with them a bit.

There's an overview of their implementation in Linux in the `man` page `mq_overview(7)`.

One annoyance of working with Posix message queues on Linux is that there doesn't seem to be a ubiquitous command-line tool for manipulating them. Their predecessor, sys v message queues, have `ipcmk`, `ipcrm`, and `ipcs`. [Apparently](https://unix.stackexchange.com/a/71045) HP-UX has an analagous tool `pipcs`, but it l


```bash
$ QFILE=`mktemp /dev/mqueue/mqtest.XXXX`
$ echo $QFILE
```

    /dev/mqueue/mqtest.a6Gp



```bash
$ ls -l $QFILE
```

    -rw------- 1 jnewsome jnewsome 80 May 10 11:07 /dev/mqueue/mqtest.a6Gp



```bash
$ cat $QFILE
```

    QSIZE:0          NOTIFY:0     SIGNO:0     NOTIFY_PID:0     



```bash
$ QNAME="/$(basename $QFILE)"
$ echo $QNAME
```

    /mqtest.a6Gp



```bash
$ compile () {
$   gcc -o $1 -xc -Wall -Werror - -lrt
$ }
```


```bash
$ read -r -d '' C_PRELIMINARIES << EOM
$ #include <fcntl.h>
$ #include <sys/stat.h>
$ #include <mqueue.h>
$ #include <stdio.h>
$ #include <stdlib.h>
$ 
$ #define CHECK_GTE0(x) { \
$   if ((x) < 0) {\
$     perror(#x);\
$     exit(EXIT_FAILURE);\
$   }\
$ }
$ 
$ #define CHECK_EQ0(x) { \
$   if ((x) != 0) {\
$     perror(#x);\
$     exit(EXIT_FAILURE);\
$   }\
$ }
$ EOM
$ 
$ compile mq_getattr <<EOF
$ $C_PRELIMINARIES
$ 
$ int main(int argc, char **argv) {
$   mqd_t q;
$   CHECK_GTE0(q = mq_open(argv[1], O_RDONLY));
$   struct mq_attr attr;
$   CHECK_EQ0(mq_getattr(q, &attr));
$   printf("flags: %ld\n", attr.mq_flags);
$   printf("maxmsg: %ld\n", attr.mq_maxmsg);
$   printf("msgsize: %ld\n", attr.mq_msgsize);
$   printf("curmsgs: %ld\n", attr.mq_curmsgs);
$ }
$ EOF
```


```bash
$ ./mq_getattr $QNAME
```

    flags: 0
    maxmsg: 10
    msgsize: 8192
    curmsgs: 0



```bash
$ compile mq_send <<EOF
$ $C_PRELIMINARIES
$ 
$ int main(int argc, char **argv) {
$   mqd_t q;
$   CHECK_GTE0(q = mq_open(argv[1], O_WRONLY));
$   struct mq_attr attr;
$   CHECK_EQ0(mq_getattr(q, &attr));
$   
$   char *buf = malloc(attr.mq_msgsize);
$   size_t n;
$   CHECK_GTE0(n = fread(buf, 1, attr.mq_msgsize, stdin));
$   CHECK_GTE0(mq_send(q, buf, n, 0));
$ }
$ EOF
```


```bash
$ ./mq_send $QNAME <<EOF
$ Hello message queue
$ Secondline
$ EOF
```


```bash
$ cat $QFILE
```

    QSIZE:31         NOTIFY:0     SIGNO:0     NOTIFY_PID:0     



```bash
$ ./mq_getattr $QNAME
```

    flags: 0
    maxmsg: 10
    msgsize: 8192
    curmsgs: 1



```bash
$ compile mq_recv <<EOF
$ $C_PRELIMINARIES
$ 
$ int main(int argc, char **argv) {
$   mqd_t q;
$   CHECK_GTE0(q = mq_open(argv[1], O_RDONLY));
$   struct mq_attr attr;
$   CHECK_EQ0(mq_getattr(q, &attr));
$   
$   char *buf = malloc(attr.mq_msgsize);
$   ssize_t n;
$   CHECK_GTE0(n = mq_receive(q, buf, attr.mq_msgsize, NULL));
$   CHECK_GTE0(fwrite(buf, 1, n, stdout));
$ }
$ EOF
```


```bash
$ ./mq_recv $QNAME
```

    Hello message queue
    Secondline



```bash
$ rm $QFILE
```
