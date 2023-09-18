# oathkeeper

Fault-tolerant distributed system in asynchronous C++

## Build and run

A pre-configured docker image is available (Ubuntu 23.04, GCC-13):

```shell
$ docker build -t agrpc-env .
$ docker run -it -v ./map_reduce:/root/map_reduce --name agrpc-env agrpc-env

(inside container) $ mkdir -p /root/build && cd /root/build
(inside container) $ cmake /root/map_reduce
(inside container) $ make -j
```

How to use the program:

```shell
(get sequential output)     $ ./sequential tests/*.txt
(run distributed MapReduce) $ ./distributed tests/*.txt
(run all unit-tests)        $ ctest
```

## Acknowledgement

Our RPC implementation is based on this awesome asynchronous gRPC C++ library:
[https://github.com/Tradias/asio-grpc](https://github.com/Tradias/asio-grpc)

Read more about the origin of
[oathkeeper](https://gameofthrones.fandom.com/wiki/Oathkeeper), one of the
finest swords in Game of Thrones.
