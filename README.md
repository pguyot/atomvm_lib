# `atomvm_lib`

This repository contains a collection of useful modules for developing programs for the AtomVM platform.

Many of these modules are "optional" and are therefore not a part of the AtomVM core libraries.

# Getting Started

The best way to use this library is to include it in your rebar3 project's `rebar.config` as a dependency:

    {deps, [
        {atomvm_lib, {git, "https://gitbub.com/atomvm/atomvm_lib.git", {branch, "master"}}}
    ]}.

Make sure to also include the `atomvm_rebar3_plugin`, so that you can generate AtomVM packbeam files and flash them to your ESP32 device.

    {plugins, [
        atomvm_rebar3_plugin
    ]}.

You can then use the `packbeam` and `esp32_flash` targets to upload your application to a device.

Some of the modules in this library make use of AtomVM components, which are native-C extensions to the AtomVM virtual machine.  For more information about these components and how to build them into the AtomVM virtual machines, see:

* [`atomvm_lib` Components](markdown/components.md)

# `atomvm_lib` modules

The `atomvm_lib` library includes the following features:

* [BME280](markdown/bme280.md) (Temperature, humidity, and pressure sensor)
* [SHT3X](markdown/sht3x.md) (Temperature and humidity sensor)
* [BH1750](markdown/bh1750.md) (Luminosity sensor)
* [LEDC PWM](markdown/ledc_pwm.md)
* [LoRa](markdown/lora.md) (SX127X transceiver)
* [HTTPd](markdown/httpd.md) (HTTP daemon)
