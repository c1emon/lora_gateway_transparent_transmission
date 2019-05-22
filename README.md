# lora-gateway

This project will drive Lora Gateway SX1301 without any unit of lorawan. The raw message can gained directly from the libloragw library.

The IOT broad is NXP i.mx6u with 256Mb RAM.


# Environment
OS : Ubuntu 16.04 desktop amd64

libs : lib32ncurses5 lib32z1 lib32ncurses5-dev lib32z1-dev

toolchain : gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux

# Introduction

* The floder lora_gateway contained all the HAL library to be used to driver SX1301.
* The floder packet_forwarder is a offical example where the process of SX1301's configuration has contained.
* The floder lora_bridge written by me aims to realize transparent transmission function.

