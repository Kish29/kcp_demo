cmake_minimum_required(VERSION 3.25)
project(kcp_demo C)

set(CMAKE_C_STANDARD 99)

add_executable(kcp_demo ikcp.c kcp_svr.c #[[kcp_cli.c]])
