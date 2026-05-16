# CIRWLib

⚠️ **WARNING:** this library is a work in progress. Listed features may NOT work as expected or may not even exist yet!

CIRWLib or _Casio, I Really Want..._ Library is a program providing all the utilities required for my Casio fx-CG50 calculator apps. This library is tested to run on an ESP32-C3 Super Mini development board with UART connections over the 3-pin serial cable to the calculator. It may work on other ESP32 chips, however you may need to modify commands/code to be compatible with your chip.

## Features

CIRWLib fundamentally extends the calcluators ability with the following:

- WiFi & Internet
- Bluetooth

Using these, CIRWLib provides these features:

- OpenAI-compatible APIs
- Wikipedia
- Wolfram|Alpha CAS

## Installation

Ensure you have [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/) installed.

Download the prebuilt binary from the [latest release](https://github.com/woody-willis/fx-cg50-esp32-CIRWLib/releases) and flash it to your ESP32-C3 board using the following command!

```bash
esptool.py --chip esp32c3 write-flash 0x0 path/to/firmware_binary.bin
```

## Build

To build a binary manually you need ESP-IDF installed on your machine and the environment activated, then follow the steps.

```bash
git clone https://github.com/woody-willis/fx-cg50-esp32-CIRWLib
cd fx-cg50-esp32-CIRWLib

idf.py build
```
