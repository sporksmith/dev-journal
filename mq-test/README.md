# Poking around at POSIX message queues in Linux

I was recently reviewing some code introduced usage of POSIX message queues into some tests. I hadn't used them before, and nothing in the code base already used them, so I needed to familiarize myself with them a bit.

One annoyance of working with POSIX message queues on Linux is that there doesn't seem to be a ubiquitous command-line tool for manipulating them. Their predecessor, sys v message queues, have [`ipcmk`](https://linux.die.net/man/1/ipcmk), [`ipcrm`](https://linux.die.net/man/1/ipcrm), and [`ipcs`](https://linux.die.net/man/1/ipcs). [Apparently](https://unix.stackexchange.com/a/71045) HP-UX has an analagous tool `pipcs`, but it hasn't been ported to other platforms.

This is somewhat mitigated by the ability to mount a virtual filesystem representing the message queues on the system, and manipulate them through normal file commands. You must be root to mount the system, but then any user can then use it. [`mq_overview(7)`](https://linux.die.net/man/7/mq_overview) has details, with an example of mounting to `/dev/mqueue`. On my system (Ubuntu 18.04), this system appears to be automatically mounted. It's unclear though whether or where you can count on it to already be mounted on other systems.

## Creating a queue

We can create a queue using normal file system APIs. Let's use `mktemp` to generate a unique name for this example:


```bash
$ QFILE=`mktemp /dev/mqueue/mqtest.XXXX`
$ echo $QFILE
```

    /dev/mqueue/mqtest.jeo0


## Examining a queue

We can `ls` it, though this doesn't tell us much:


```bash
$ ls -l $QFILE
```

    -rw------- 1 jnewsome jnewsome 80 May 11 08:50 /dev/mqueue/mqtest.jeo0


We can `cat` it, giving some metadata about the state of the queue:


```bash
$ cat $QFILE
```

    QSIZE:0          NOTIFY:0     SIGNO:0     NOTIFY_PID:0     


To get more information, we'll need to use the C api.

The C APIs take a queue name, not the the file system path. The queue name is the basename of the path, with a leading `/`:


```bash
$ QNAME="/$(basename $QFILE)"
$ echo $QNAME
```

    /mqtest.jeo0


Let's write a small shell function to compile our test programs. It'll take the program source from `stdin`:


```bash
$ compile () {
  gcc -o $1 -xc -Wall -Werror - -lrt
}
```

Our helper programs are going to have a common preamble. Let's stick those in a shell variable:


```bash
$ read -r -d '' C_PRELIMINARIES << EOM
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) { \
  if (!(x)) {\
    perror(#x);\
    exit(EXIT_FAILURE);\
  }\
}

#define CHECK_GTE0(x) CHECK((x) >= 0)
#define CHECK_EQ0(x) CHECK((x) == 0)

EOM
true # Suppress error
```

Now let's write and compile our `mq_getattr` program:


```bash
$ compile mq_getattr <<EOF
$C_PRELIMINARIES

int main(int argc, char **argv) {
  mqd_t q;
  CHECK_GTE0(q = mq_open(argv[1], O_RDONLY));
  struct mq_attr attr;
  CHECK_EQ0(mq_getattr(q, &attr));
  printf("flags: %ld\n", attr.mq_flags);
  printf("maxmsg: %ld\n", attr.mq_maxmsg);
  printf("msgsize: %ld\n", attr.mq_msgsize);
  printf("curmsgs: %ld\n", attr.mq_curmsgs);
}
EOF
```

Running it gives us some new information about the queue:


```bash
$ ./mq_getattr $QNAME
```

    flags: 0
    maxmsg: 10
    msgsize: 8192
    curmsgs: 0


## Writing to the queue

Unfortunately there doesn't seem to be a way to write messages to the queue via the file system API. At least, attempting to do so in the obvious way fails:


```bash
$ echo "A message" > $QFILE || true
```

    bash: echo: write error: Invalid argument


Let's write another small program that'll read up to the maximum message length from `stdin`, and then write it to the queue with a specified priority:


```bash
$ compile mq_send <<EOF
$C_PRELIMINARIES

int main(int argc, char **argv) {
  CHECK(argc == 3);
  const char* qname = argv[1];
  unsigned int priority = atoi(argv[2]);
  
  mqd_t q;
  CHECK_GTE0(q = mq_open(qname, O_WRONLY));
  struct mq_attr attr;
  CHECK_EQ0(mq_getattr(q, &attr));
  
  char *buf = malloc(attr.mq_msgsize);
  size_t n;
  CHECK_GTE0(n = fread(buf, 1, attr.mq_msgsize, stdin));
  CHECK_GTE0(mq_send(q, buf, n, priority));
}
EOF
true # Suppress error
```

...and use it to write a message to the queue:


```bash
$ ./mq_send $QNAME 0 <<EOF
Hello message queue
Second line
EOF
```

Examining the queue, we can see that there's a message there:


```bash
$ cat $QFILE
```

    QSIZE:32         NOTIFY:0     SIGNO:0     NOTIFY_PID:0     



```bash
$ ./mq_getattr $QNAME
```

    flags: 0
    maxmsg: 10
    msgsize: 8192
    curmsgs: 1


## Reading from the queue

Again, we'll need to write a program to read from the queue. This one will just cat the first message in the queue to `stdout`, blocking if there isn't a message there yet:


```bash
$ compile mq_recv <<EOF
$C_PRELIMINARIES

int main(int argc, char **argv) {
  mqd_t q;
  CHECK_GTE0(q = mq_open(argv[1], O_RDONLY));
  struct mq_attr attr;
  CHECK_EQ0(mq_getattr(q, &attr));
  
  char *buf = malloc(attr.mq_msgsize);
  ssize_t n;
  CHECK_GTE0(n = mq_receive(q, buf, attr.mq_msgsize, NULL));
  CHECK_GTE0(fwrite(buf, 1, n, stdout));
}
EOF
```

As expected, we can read out the message we wrote to the queue above:


```bash
$ ./mq_recv $QNAME
```

    Hello message queue
    Second line


...and doing so returns the queue to its empty state:


```bash
$ cat $QFILE
```

    QSIZE:0          NOTIFY:0     SIGNO:0     NOTIFY_PID:0     



```bash
$ ./mq_getattr $QNAME
```

    flags: 0
    maxmsg: 10
    msgsize: 8192
    curmsgs: 0


## Multiple messages

We can write two messages to the queue and get them back individually:


```bash
$ ./mq_send $QNAME 0 <<EOF
First message first line
First message second line
EOF
./mq_send $QNAME 0 <<EOF
Second message first line
Second message second line
EOF
```


```bash
$ ./mq_recv $QNAME
```

    First message first line
    First message second line



```bash
$ ./mq_recv $QNAME
```

    Second message first line
    Second message second line


We can use priorities to get them back in a different order. We didn't talk about priorities earlier, but that's the integer passed on the command-line after the queue name. When reading from a queue, the earliest message with the highest priority is returned:


```bash
$ echo "First message, priority 0" | ./mq_send $QNAME 0
$ echo "Second message, priority 0" | ./mq_send $QNAME 0
$ echo "Third message, priority 1" | ./mq_send $QNAME 1
$ echo "Fourth message, priority 1" | ./mq_send $QNAME 1
$ ./mq_recv $QNAME
$ ./mq_recv $QNAME
$ ./mq_recv $QNAME
$ ./mq_recv $QNAME
```

    Third message, priority 1
    Fourth message, priority 1
    First message, priority 0
    Second message, priority 0


## Resource usage and cleanup

There are system-wide and per-user limits on message queues, and these are surprisingly small by default. Let's go ahead and clean up our queue from above:


```bash
$ rm $QFILE
```

While writing this I inadvertently bumped into the per-user limit when I was unable to create a 10th queue:


```bash
$ for i in {1..10}; do touch /dev/mqueue/exhaust${i}; done
true # Suppress error
```

    touch: cannot touch '/dev/mqueue/exhaust10': Too many open files


I believe the limit we're hitting is the per-user limit on message queues in bytes:


```bash
$ ulimit -a | grep POSIX
```

    POSIX message queues     (bytes, -q) 819200


As we saw from the `mq_getattr` output above, each queue has `msgsize` of `8192`, and `maxmsg` of `10`. Therefore `10` queues would equal the `819200` limit. Apparently the limit is applied to the *potential* size of the queue. This is probably preferable over being able to create more queues and then hitting the resource limit when trying to write to them.

It does seem likely that these parametes were chosen with the intent of allowing `10` queues per user, but that either the limit is being applied with `<` rather than `<=`, or that there's some other per-queue overhead is also being counted.

In any case, let's clean up.


```bash
$ rm /dev/mqueue/exhaust*
```
