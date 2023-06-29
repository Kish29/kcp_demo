#! /bin/bash

llvm-gcc -o demo_svr ikcp.c kcp_svr.c
llvm-gcc -o demo_cli ikcp.c kcp_cli.c
