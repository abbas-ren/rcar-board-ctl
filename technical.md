## 1. Purpose

`boardctl` is a C++ command-line utility for lab automation on Raspberry Pi 4/5 hosts.

It provides:
- relay control over USB FTDI relay boards
- GPIO pin control using libgpiod
- UART monitor/send support
- Gen4 IPL flashing over UART
- script execution without shell interpolation
- Gen5 power control through a USB serial power interface

This implementation was designed from the attached C codebase

## 2. Source-driven behavior

This implementation follows the attached source behavior:

- Relay board:
  - USB VID/PID = `0x0403:0x6001`
  - bulk out endpoint = `0x02`
  - device located by USB serial number
  - bitbang mode enabled via vendor control transfer `0x0B`
  - relay state read via vendor control transfer `0x0C` [1][9]

- GPIO:
  - original code uses `libgpiod`
  - opens `/dev/gpiochip0`
  - drives two output pins together [2]

- Gen4 IPL:
  - UART configured for `921600`
  - FlashWriter file:
    `ICUMX_Flash_writer_SCIF_DUMMY_CERT_EB203000_V4H.mot`
  - flashes eight images to Flash/eMMC
  - waits for prompts such as:
    - `please send !`
    - `>`
    - `Select (1-3)>`
    - `EM_W Complete!`
    - `complete!`
    - `Clear OK?(y/n)` [4]

- USB TTY discovery:
  - scans `/sys/bus/usb/devices`
  - prefers interface `:1.0`
  - maps to `/dev/ttyUSB*` [5]

- Gen5:
  - power interface logic uses `POWER#ON` and `POWER#OFF`
  - original code uses shell echo into power TTY [3]
  - this implementation writes directly to the serial device instead

## 3. Build requirements

Install dependencies:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libusb-1.0-0-dev \
  libgpiod-dev
```

### Build

```bash
cmake -S . -B build
cmake --build build -j
```
### Binary Output:

```bash
./build/boardctl
```


## 4. CLI reference

### Relay control

```bash

# Turn channel on:
./build/boardctl relay set --serial FT123456 --channel 0 --state on

# Turn Channel off
./build/boardctl relay set --serial FT123456 --channel 0 --state off

# Read single channel status
./build/boardctl relay status --serial FT123456 --channel 0
```

#### Behavior:

- reads current relay mask
- modifies only the selected channel bit
- writes new state back to FTDI relay

### GPIO control

```bash
./build/boardctl gpio set --chip /dev/gpiochip0 --pins 17,27 --value 1
./build/boardctl gpio set --chip /dev/gpiochip0 --pins 17,27 --value 0
```

#### Behavior:

- requests all specified lines as outputs
- sets all provided pins to the same value
- request is released when command exits

### UART monitor

```bash
./build/boardctl uart monitor --device /dev/ttyUSB0 --baud 921600
```
### UART send

```bash
./build/boardctl uart send --device /dev/ttyUSB0 --baud 115200 --text "help\r\n"
```

### USB discovery

```bash
./build/boardctl usb discover --vid 0403 --pid 6010
```

### Script execution

```bash
./build/boardctl script run --path ./tests/smoke.sh -- --board gen4 --verbose
```

#### Behavior:

- uses `fork` + `execvp`
- does not invoke a shell
- returns child exit code

### Gen4 IPL flash

```bash
./build/boardctl ipl flash-gen4 \
  --uart /dev/ttyUSB0 \
  --relay-serial FT123456 \
  --relay-channel 2 \
  --gpio-chip /dev/gpiochip0 \
  --gpio-pins 17,27 \
  --path ./ipl/gen4 \
  --profile ./profiles/gen4.toml
```

### Gen5 power control

```bash
./build/boardctl gen5 power on --device /dev/ttyUSB2
./build/boardctl gen5 power off --device /dev/ttyUSB2
./build/boardctl gen5 power reboot --device /dev/ttyUSB2
```

## 5. Module responsibilities

### `relay.cpp`
Implements FTDI relay communication.

Key operations:
1. initialize libusb
2. enumerate USB devices
3. match VID/PID and serial number
4. claim interface 0
5. enable bitbang mode
6. read current state byte
7. update selected relay bit
8. bulk-write one byte to endpoint `0x02` [1][9]

### `gpio.cpp`
Implements Raspberry Pi GPIO control using `libgpiod`.

Key operations:
1. open chip path
2. create line settings/config/request config
3. request one or more output lines
4. set output values
5. release resources [2]

### `uart.cpp`
Implements serial-port access using POSIX `termios`.

Key operations:
- open serial device
- set baud
- raw mode configuration
- read with timeout-like polling
- send text commands
- send SREC files line by line [4]

### `usb_scan.cpp`
Implements `/sys/bus/usb/devices` scanning.

Key operations:
- read `idVendor`
- read `idProduct`
- locate preferred `:1.0` interface
- discover `/dev/ttyUSB*` node [5]

### `profile.cpp`
Loads a minimal known TOML profile for Gen4 flashing.

### `ipl_gen4.cpp`
Implements the Gen4 flashing state machine.

### `process.cpp`
Runs test scripts or helper processes using `execvp`.

## 6. Relay protocol details

The relay hardware behavior can be directly inferred from the attached code:

- VID = `0x0403`
- PID = `0x6001`
- bulk endpoint = `0x02`
- state read uses:
  - request type `0xC0`
  - request `0x0C`
- bitbang enable uses:
  - request type `0x40`
  - request `0x0B`
  - value `0x01FF`
  - index `0x01` [1][9]

The relay state is one byte.

Bit mapping:
- channel 0 -> bit 0
- channel 1 -> bit 1
- ...
- channel 7 -> bit 7

To switch one channel:
1. read current byte
2. set/clear one bit
3. write byte back [1]

## 7. GPIO design

The original implementation stores a global `chip` and `request`, requests exactly two lines, and sets both together [2].

This C++ implementation is simpler and safer for CLI usage:
- request is created inside the command
- all requested pins are set together
- request is released automatically when the command exits

This design is good for one-shot CLI use.

If you later want a long-running daemon mode, you can extend this with a persistent request object.

## 8. Gen4 IPL state machine

The original Gen4 implementation does the following [4]:

1. open UART
2. switch to `921600`
3. send FlashWriter `.mot`
4. wait for `>`
5. for each image:
   - if target is Flash:
     - send `xls2`
     - select option `1`
     - confirm with `y`
     - send load address
     - send save address
     - wait for `please send ! (Motorola S-record)`
     - send SREC
     - wait for `complete!` or `Clear OK?(y/n)`
   - if target is eMMC:
     - send `em_w`
     - confirm with `y`
     - select area `1`
     - send sector/start address
     - send program address
     - wait for `please send ! (Motorola S-record)`
     - send SREC
     - wait for `EM_W Complete!`

This implementation preserves that same flow [4].

## 9. Download-mode sequencing

The HTTP server code shows that Gen3/Gen4 download mode is entered by:

1. relay off
2. set GPIO boot pins
3. relay on
4. wait for SCIF prompt [3]

This tool implements the same sequence before flashing and restores normal GPIO mode afterward.

## 10. Gen4 profile format

The profile file contains:
- board name
- UART baud
- expected prompt strings
- FlashWriter filename
- image list
- target type (`flash` or `emmc`)
- save/load addresses

Example:

```toml
[board]
name = "gen4"
uart_baud = 921600
download_prompt = "please send !"
flashwriter_prompt = ">"

[flashwriter]
file = "ICUMX_Flash_writer_SCIF_DUMMY_CERT_EB203000_V4H.mot"

[[images]]
file = "bootparam_sa0.srec"
target = "flash"
save_address = "0"
load_address = "EB200000"
```

## 11. Gen5 notes

The source code shows two different Gen5-related behaviors:

1. direct power control:
   - send `POWER#ON`
   - send `POWER#OFF` [3]

2. full Gen5 IPL:
   - delegated to external script `gen5_ipl.sh` [3]

This C++ tool currently implements direct power control only.

If you want, the next C++ revision can add:

```bash
boardctl ipl flash-gen5 --script ./gen5_ipl.sh --sdk-version X --path Y --power /dev/ttyUSB2 --uart /dev/ttyUSB3
```

## 12. Safety improvements over original code

The original code uses several shell commands, including:
- `system("echo ... > tty")`
- `system("rm -rf ...")`
- `system("fuser ... | xargs kill")`
- `popen("./gen5_ipl.sh ...")` [3][4][6][7]

This implementation improves safety by:
- writing directly to UART/power devices
- executing scripts via `execvp`
- validating relay channel range
- using RAII-style cleanup for libusb handles and GPIO requests
- using `std::filesystem::path` for file paths

## 13. Extending the code

### Add more relay commands
Possible future additions:
- `relay all-on`
- `relay all-off`
- `relay mask --value 0x3F`

### Add persistent GPIO sessions
Current implementation is one-shot.  
A persistent mode could hold line requests and support repeated operations.

### Add more board profiles
You can add:
- `profiles/gen3.toml`
- `profiles/gen5.toml`

Then dispatch by profile name or `--board`.

### Add automatic UART resolution
Current Gen4 flash command takes explicit `--uart`.
You can extend it to resolve the UART automatically by MAC + relay mapping logic similar to the original code [7].

## 14. Known limitations

1. Minimal TOML parser
   - current parser is intentionally lightweight
   - assumes the known file structure
   - if you want fully generic TOML support, add `toml++`

2. No built-in Gen5 script wrapper yet
   - power control exists
   - full script-driven flash wrapper can be added next

3. No RTOS capture endpoints yet
   - original HTTP service supports RTOS log capture [3]
   - CLI version does not yet expose `rtos start` / `rtos stop`

4. No background daemon/API
   - this is a CLI utility only

## 15. Suggested test plan

### Relay
- verify each channel individually
- confirm other channels are preserved

### GPIO
- verify pin voltage with multimeter or logic analyzer
- test both normal and download mode pin combinations

### UART
- monitor known boot logs
- confirm prompt matching at 921600

### Gen4 flash
- verify SCIF prompt appears
- verify FlashWriter prompt `>`
- verify all 8 images program successfully
- verify reboot into normal mode afterward

### Gen5 power
- confirm `POWER#OFF` and `POWER#ON` actually toggle target power

## 16. Next recommended enhancements

If you want the C++ version to become production-grade, I recommend next:

1. add structured logging to file
2. add `--timeout` options
3. add `--json` output mode
4. add `ipl flash-gen5`
5. add `rtos start` / `rtos end`
6. add automatic UART lookup from MAC/relay mapping
7. replace minimal TOML parsing with `toml++`