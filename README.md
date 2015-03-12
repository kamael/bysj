# bysj

compile:
```
gcc -o server server.c -pthread
gcc -o client client.c
```

run:
```
./server 7555
./client 127.0.0.1 7555 456789
```
