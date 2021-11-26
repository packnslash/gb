# gb
Completely non-unique DMG emulator

CAN SOMEONE TELL ME HOW TO LINK SDL2 INTO A MAKE FILE USING POWERSHELL AND SDL2 from source ???
Until then, build it yourself :)

## Features
    - Fairly accurate CPU (passes blarggs cpu_instrs)
    - Decent PPU (passes mattcurie dmg-acid2)
    - MBC 1,2,3 (no RTC)
## Bugs
    - Objects don't take into account the objects underneath their transparent pixels (seen in Pokemon Blue Version title screen) 
    - LCD Enable isn't implemented for some reason
## TODO
    - Perform cycle accuracy tests
    - Perform mooneye cpu tests
    - Sound
    - Gamepad support
    - More MBC implementations
    - Support RTC
