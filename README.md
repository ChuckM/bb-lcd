Readme
======

This is a pretty simple demo of using the Embest LCD expansion board with
the [STM32F4-Butterfly][stm]. The LCD expansion [(STM32F4DIS-LCD)][lcd] connects
to the Embest "Base Board"[(STMF32F4DIS-BB)][bb] via a 20 pin connector 
and has an Solomon Systech SSD2119 LCD controller on it. These boards are 
sold by Element14/Newark in the US and Farnell in the UK. Element14 also includes
some [software examples][d2]

Sadly there isn't a lot of documentation on the LCD card, what we have is a
fairly rambling "test" demo that came from Embest, some board schematics, and
a somewhat creatively translated data sheet for the LCD controller chip. 

What you need to know to understand this code is that the LCD is connected to
the STM32F4 using the flexible static memory controller (FSMC) in a weird sort
of way which turns it into a 16 bit wide parallel port. Most of the work getting
the demo running was figuring out this part of the setup. The FSMC is configured
as a Static RAM port, bank 1 is enabled (which shows up in the address space at
0x60000000) and only 1 address line is used (A19) which is connected to the DC
(Data/Command\*) pin to select between writing data to the LCD or a command.

The folks over at [AdaFruit][ada] put together a really simple
[graphics library][gfx] which I borrowed, converted into C, and checked in
here as gfx.c. It has basic primitives like draw a line etc, I added a
`gfx_puts(char *)` which writes a string to the LCD. 

Other files and their explanation;

* uart.c - these are the uart routines. They use USART2 but you could easily
  change that to USART6 if you wanted to use the uart on the baseboard as your
  connection.

* lcd.c - this is the meat of this demo. Some of the key functions are:
  - `lcd_setup()` sets up various GPIO pins to their alternate function 
     (FSMC).
  - `lcd_init()` basically replicates the series of commands that Embest
    used to initialize the LCD controller.

    I don't have a lot of visibility into the actual panel so I haven't
    done a lot of experimenting with the various choices made. 
  - `lcd_write_pixel()` is the thing that actually changes a pixel's color
    and it has a small enhancement in that it avoids a command cycle when
    the pixel its writing is 'next' to the previous one (the LCD auto
    increments its location counters).

* gfx.c - this is my simple port of the Adafruit code, basically the standard
  change from Cpp to C is create a structure to hold state, prefix the methods
  with a name (gfx\_) and your done. The only dependency outside of its include
  file is the `lcd_write_pixel()` function.

* util.c - this sets up some basic board stuff, and implements the SysTick
  handler to give reasonable timing delays. 

[stm]: http://www.st.com/web/catalog/tools/FM146/CL1984/SC720/SS1462/PF255417
[bb]: http://www.newark.com/stmicroelectronics/stm32f4dis-bb/dev-kit-cortex-m4f-stm32f4xx-discovery/dp/47W1731
[lcd]: http://www.newark.com/stmicroelectronics/stm32f4dis-lcd/daughter-card-3-5inch-touch-screen/dp/47W1734
[d2]: http://www.element14.com/community/docs/DOC-51670/l/stm32f4dis-bb-discover-more-software-examples
[ada]: http://learn.adafruit.com/adafruit-gfx-graphics-library/overview
[gfx]: https://github.com/adafruit/Adafruit-GFX-Library

