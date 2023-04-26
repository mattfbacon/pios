#set page(
	paper: "us-letter",
)
#set par(
	justify: true,
)
#show link: underline

= Timjudmi'i

_An Interactive Weather Station; Bare-Metal on the Raspberry Pi 4B_

Matthew Fellenz

== Introduction

Timjudmi'i is an interactive weather station that:

- Measures temperature and humidity on a regular basis,
- Uses a real-time clock for accurate sampling,
- Presents a rudimentary textual user interface on a 16x2 LCD panel that allows querying the data, setting the time, and resetting the database,
- Uses an SQLite relational database stored on a MicroSD card to store and query the data,
- And does this all while running bare-metal directly on the hardware!

This document will outline the motivations (why) and methodology (how) in creating this project.

The source code is available for your viewing pleasure at #link("https://github.com/mattfbacon/pios"). The code for the weather station is in the `weather-station` branch.

== Motivation

Knowing that software in a typical operating system runs in a restricted and managed context, I was curious what it would take to interact directly with the hardware outside that context. By undertaking this project I learned in-depth the techniques and idiosyncrasies necessary to communicate with diverse hardware to create the single unified system of a weather station.

== Context

The Raspberry Pi 4B is an AArch64 Single-board computer (SBC). It presents a fairly complete implementation of ARMv8 and its many associated extensions. It is based on the Broadcom BCM2711 chip which hosts a plethora of useful peripherals, such as GPIO, UART, I2C, SPI, PWM, EMMC, timers, and interrupt controllers. All these peripherals present a register-based memory-mapped interface that the software can interact with.

When the Raspberry Pi boots, the GPU is the first component to start execution. It loads the bootloader, which then reads its configuration and loads the operating system based on the configuration, in two stages: the "ARM stub" and the main operating system. (The ARM stub is meant to allow presenting a uniform hardware initialization state to the main operating system such that quirks in initialization can be papered over.) The software provided by the user is the main operating system and sometimes the ARM stub.

== Design

=== Modular Unikernel

("unikernel" is defined as "specialized, single-address-space machine images constructed using library operating systems".)

The operating system itself is designed to be as generic as possible. Development is divided into two branches: `main` and `weather-station`. `main` has no application code, but rather provides library routines to interact with most of the hardware provided by the device. `weather-station` is based on `main` and contains the application code for the weather station.

This means that other developers interested in using the library routines as a basis for their own bare-metal project do not have to painstakingly separate the application code from the library code, and can start with the "clean slate" presented by `main`.

=== Generic

As a general approach, the library routines are designed to be as generic as possible with respect to usage patterns and configuration. Some concessions have been made where allowing configuration would excessively increase implementation complexity (for example, allowing the LCD to operate in different modes besides 16x2, or allowing the real-time clock to switch between 12- and 24-hour mode).

This means that if another project (or another realization of the same project or a component thereof) wants to use a peripheral in a slightly different way, the developers will not have to dig through the library code to find and update the assumptions on a certain operating mode.

=== Lightweight

Compared to an operating system like Linux, the unikernel interface provided by this is almost so lightweight as to be trivial. There is no scheduler, no processes or threads, no context-swiitching (except to handle interrupts), and most importantly no supervisor or manager to restrict the capabilities of the software. This allows, at least theoretically, much more optimized code.

== Development Process

In my mind I divide the development process into a few main stages, as follow:

=== First Steps

Initially I simply wanted a "sign of life" so I made it just turn the on-board ACT LED (GPIO pin 42) on. At this point the code was a single C file and I was building it manually. This worked and I was thrilled!

The next step was to implement "sleeping", which I did by spinning for a certain number of relatively slow instructions (`isb`; this approach still remains in the codebase as `sleep_cycles`). With this I could blink the LED instead of just turning it on.

Marking the start of my general approach to this project, I extracted the GPIO interfacing code into a module and refactored the code to use that.

=== Driver Proliferation

As my curiosity progressed, I continued to implement drivers for other peripherals. The first and highest priority was the UART, so I could send debug information to my development environment. As far as memory-mapped interfaces go, the UART interface presents a medium level of complexity (GPIO was one of the easiest). As such it was a good second driver for me to implement.

The initial implementation had a couple bugs due to my lack of familiarity with memory-mapped interfaces in general, but I quickly ironed them out and that UART driver code remains in the codebase today!

It was also around this stage that I encountered my first hardware quirk: the "mini UART" only works if you plug in an HDMI display! This is because the UART clock is tied to the CPU frequency, which is lowered when there is a display attached. As far as I can tell, this is unavoidable, so I don't use the mini UART at all.

At this point one of my main resources was #link("https://www.rpi4os.com"). Thus, in the progression of my driver development I generally followed its progression, interacting with the framebuffer and playing sound on the 3.5 mm jack using PWM. I skipped some parts that I wasn't interested in, such as bluetooth. I also corresponded with the author over email.

=== Low-Level Stuff; Here Be Dragons

At some point I realized that the setup provided by the default ARM stub was inadequate. For one, it was setting up the "generic interrupt controller" (GIC). This apparently new and more modern interrupt controller interferes with the "legacy" controller (legacy in this case meaning well-documented and usable). As such, I had to create my own ARM stub to override it.

Around this time I also encountered my first "low-level" bug: my integer-to-string routine was causing the system to halt. Since I had interrupts set up (using the legacy controller), I was able to install a handler that would catch the CPU exception and tell me what went wrong. It turned out that the compiler had optimized my function to use SIMD instructions for copying some bytes around, and the CPU was set up to "trap" these instructions and raise an exception.

Figuring out how to disable this trapping was not at all straightforward. (It didn't help that the official ARM docs are some of the worst I've ever used.) Eventually I found the "model-specific registers" (MSRs) that I had to set up to allow the instructions.

In order to take interrupts properly, I also had to switch "exception levels" (ELs). This was fairly complicated as well, and brought many similar issues as I figured out which MSRs I had to set up before and after this switch to give my code the proper capabilities such as executing floating-point instructions.

Another hardware quirk: ARM is in fact capable of performing unaligned memory accesses, but requires the MMU to be enabled to allow them. Besides working around this quirk, I had no use at all for the MMU, so I hadn't set it up at this point. Setting up the page tables and enabling the MMU was arduous but luckily I found a few good references and examples that helped me.

== Back to Drivers

So finally, I was running in EL1 (the typical EL for OSes) and could move on to more interesting prospects. I implemented drivers for some of the devices I planned to use, such as the AHT20 temperature and humidity sensor, the DS3231 real-time clock, the MCP23017 I2C GPIO expander, and the HD44780U LCD controller. The main issue I encountered here was timing: many peripherals require you to ensure certain pauses between different actions, but don't document these very well, so I had to determine a lot of them from testing. Turns out figuring out whether a timing bug has even occurred is not straightforward! But I figured it all out and I'm pretty sure my determined sleep times are fairly close to the "specification".

The final, and by far the most difficult, driver I had to implement was EMMC, so I could store the database on the microSD card. The EMMC controller's MMIO (memory-mapped IO) interface for the BCM2711 is completely undocumented, but the BCM2853#sym.quote.r.single;s interface is documented, and they're apparently the same, except for a different base address! Also, I was able to rely on some good tutorials, such as #link("https://invidious.baczek.me/watch?v=mCuxbI0FqzM"), and ended up with a working driver in my own code.

=== SQLite Integration

Now that I had the EMMC driver implemented, I could start integrating SQLite itself. I started by adding the SQLite "amalgamation" files (a single source and header file for the whole library) to the repository and the build system. However, since SQLite by default tries to use the operating system's interface for file IO, time, etc, I had to set some compile-time flags like `SQLITE_OS_OTHER` to tell it that this is not a known OS.

Disabling the built-in "virtual file systems" (VFSes; the interface between SQLite and the operating system), though, means that I have to implement my own.

The main purpose of the VFS is to present a filesystem interface. You may think, then, that I would have to implement a filesystem. Well, not really, because I only need to store one "file": the main database. (I disabled journaling and all other functions that require separate files.) Thus, I can get away with a much simpler approach: dividing the entire partition into a small header to store the size of the "file" and then just a big chunk of raw data.

The VFS also includes a couple of other interfaces like getting the time and getting random numbers, but I don't use any of those functions so I just stubbed them to always return an error.

With this custom VFS set up, I was able to start using SQLite! I tested it a bit with an interactive database prompt over the UART.

=== Weather Station: Bringing it all together

Conceptually, the weather station is organized into two components: gathering data and showing the user interface. In typical unikernel fashion, the code is organized with the "one big loop" technique. The data gathering is triggered by a timer, and the user interface is triggered by LCD buttons. As such, the loop waits until one of these events occurs before repeating.

The UI itself is split into two stages: update and render. This is done so that updates to the interface are immediately visible. As such, defining a "widget" requires defining separate functions for rendering and updating (if applicable; non-interactive widgets only require showing).

Altogether, the main loop is able to sleep (and this is a true sleep where the CPU is idle and waiting for interrupts) for most of the time, allowing for power-efficient operation.

== Final Hardware

#image("schematic.svg", width: 40%)

The LCD shield can be purchased at #link("https://www.adafruit.com/product/714"). It is an MCP23017 I2C GPIO expander connected to an HD44780U LCD controller, along with five buttons.

The DS3231 can be purchased at #link("https://www.adafruit.com/product/3013").

The AHT20 can be purchased at #link("https://www.adafruit.com/product/5183").

== Final Software

The final source code is available at #link("https://github.com/mattfbacon/pios/tree/weather-station").

== Usage Guide

=== Setup

1. Create two partitions on an SD card. The GPT partitioning scheme should be used. The first partition will be FAT32 with the "piboot" label for the boot partition. The second partition can be unformatted.
1. Put the required Raspberry Pi boot files in the boot partition.
1. Update the expected GUID in `src/main.c` to reflect the GUID of the second partition, which will be used for the weather data.
1. Run the `autoupload.sh` script and plug in the SD card to your system. The script should automatically upload the required files.
1. Attach the hardware as described in the schematic.
1. Boot the Pi from the SD card in the SD card slot (no USB adapters or anything like that).
1. The LCD should light up and begin showing the weather data as well as the time. Set the time if necessary and reset the database to ensure entries with the wrong time are purged. (See the next section for how to do this.)

=== Operation

In the default mode, the LCD will alternate between the time and the sensor data.

When interacting with menus, Left will exit/cancel/go back, Right and Select will submit/continue, and Up and Down will modify/scroll. All of the menus follow this scheme.

=== Video and Images

Reference this video for a more in-depth usage guide and demonstration: #link("https://inv.riverside.rocks/watch?v=YGMFd1Fx_ZE")

#image("img1.jpg")

#image("img2.jpg")

== Remarks

Using C for this project was painful to say the least. Especially in the GUI code where there are a lot of janky `union`s and `enum`s it's hard to be sure that my code is completely correct and memory-safe. I had to debug a lot of annoying `NULL`-pointer dereferences (which is just another pointer for us) and that type of thing that simply wouldn't have occurred if this was done in Rust.

That said, using C also made a lot of this project quite simple, such as integrating SQLite.

== Future Possibilities

Considering the modular nature of the unikernel, the same platform could be used for many other projects. Some ideas I've had include:

- An audio player that reads audio files off of the SD card and uses the LCD to present an interface that lets you choose the track, play, pause, etc.
- A dynamic scripting/shell environment presented over UART that allows interacting with all of the peripherals dynamically.
- A "bootloader" of sorts that receives programs over the UART and executes them, to avoid having to constantly remove and replace the SD card.
- A sequencer/music maker program using the LCD.
- A thermostat using the real-time clock and the temperature/humidity sensor that could be infinitely programmable with rich and complex logic, and which could further present an interface on the LCD to customize behavior at runtime.
- Extending the weather station with a UI shown on an HDMI monitor that can display various information and possibly be interactive using buttons or even Bluetooth keyboards/mice (#link("https://www.rpi4os.com") has a part for this--it used a Bluetooth keyboard and the HDMI display to make Breakout)
