# Set up Task Pop

## Hardware

Tested on **LCDWIKI E32R28T / ESP32-32E**, with 4 MB flash, a 240×320 ILI9341V screen, XPT2046 resistive touch, and an onboard common-anode RGB LED. Other CYD variants may need different pins or display initialization.

| Component | Connections |
| --- | --- |
| Display, HSPI | MOSI 13, MISO 12, SCK 14, CS 15, DC 2, backlight 21 |
| Touch, separate VSPI | MOSI 32, MISO 39, SCK 25, CS 33 |
| RGB LED | Red 22, green 16, blue 17; active low |

Both the display and touch use portrait rotation **0**, with USB at the bottom. [Board documentation](https://www.lcdwiki.com/2.8inch_ESP32-32E_Display).

## Build from source

Use **Arduino CLI** with:

- ESP32 Arduino core **2.0.14**
- TFT_eSPI **2.5.43**
- XPT2046_Touchscreen (Paul Stoffregen)

`Preferences` and `SPI` come with the ESP32 core. Clone or extract the source into a folder named **TaskPop**, so it matches `TaskPop.ino`.

On macOS or Linux:

```sh
bash build.sh
bash build.sh /dev/cu.usbserial-110
```

Replace the serial port with your board's port. Set `ARDUINO_CLI` if the executable is outside your PATH. On macOS, the script can also use the copy bundled with Arduino IDE.

The script supplies `TaskPop_Display.h` to TFT_eSPI for this build. Opening the sketch in Arduino IDE alone does not apply that configuration.

For a manual build, run from the TaskPop folder and use your full path to the header:

```sh
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,UploadSpeed=115200 \
  --build-property 'compiler.cpp.extra_flags=-DUSER_SETUP_LOADED -include /absolute/path/TaskPop/TaskPop_Display.h' \
  .
```

## Install the ready-built firmware

Download and extract `TaskPop-firmware-v1.3.zip`. With esptool installed, run this from the extracted folder, replacing the port:

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-110 --baud 115200 \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x1000 TaskPop.ino.bootloader.bin \
  0x8000 TaskPop.ino.partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 TaskPop.ino.bin
```

For an existing Task Pop installation with the same partition layout, an application-only update preserves saved tasks and calibration:

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-110 --baud 115200 \
  write-flash 0x10000 TaskPop.ino.bin
```

A full flash erase removes saved tasks and calibration. The downloadable firmware contains no saved task data.

## Everyday use

- **+** opens the keyboard; **Save** or **OK** adds the task.
- A checkbox toggles completion. Tap the text to edit or delete.
- **Undo** is available for eight seconds after deletion.
- Store up to **30 tasks**, each up to **64 characters**, across pages of three.
- The keyboard supports English letters, numbers, and symbols.
- For touch alignment, tap **TOUCH** and press the center of each of the four targets. Calibration is saved automatically.
- Use a deliberate press or a suitable stylus on the resistive touchscreen.
- Tap the pink **FOCUS** button for the Pomodoro timer. Choose 1–90 minutes with **−/+**, or use the 5/25/50-minute presets, then tap **START FOCUS**.
- **Pause/Resume** preserves remaining time. **Stop** returns directly to the task list.
- When the countdown finishes, the screen and RGB LED pulse until **BACK TO TASKS** is pressed. The selected duration is saved; an active session resets after power loss.

## Files

`TaskPop.ino` contains the interface and task storage. `LedFlow.h` controls the RGB animations without blocking the interface. `FocusTimer.h` handles monotonic countdown timing, pause/resume, and the completion alert. `TaskPop_Display.h` holds the board-specific display configuration.

The software was checked on the device for task entry, editing, completion, deletion, undo, paging, persistence, RGB event output, and display rotation. Screenshots show sample tasks.
