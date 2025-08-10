# EEPROM Programmer

## Overview:
A parallel EEPROM programmer based off of the one [Ben Eater](https://github.com/beneater/eeprom-programmer) made on [YouTube](https://www.youtube.com/watch?v=K88pgWhEb1M) but for a Pi Pico instead of an Arduino Nano (I only had a Pi Pico at the time). 

Uses a Python script (included in this repo) to interface with it over serial connection to send data to be programmed.

Designed to work with parallel EEPROMs with up to 15 address lines and 8 data lines. This worked with an AT28C256 [link](https://mou.sr/3UmaxqV) (Mouser).

The EEPROM has to be powered with +5v for the programming to work but I found that the data and address lines only have to be 3.3v which is what the Pico can output.


## Write Instructions:

In the initial state the programmer will expect the data in 4 bytes in the following format `[instruction] [address high] [address low] [data]`. Sending the data `0x01 0x01 0x2f 0xaa` will write the data `0xaa` to address `0x012f` and then wait for the next instruction after acknowledging.

When this changes is when the programmer is set to byte stream mode. Then the programmer will expect every incoming byte to be a data byte and write sequentially to the starting address.

For example, sending the instruction `0x02` initiates the programmer into byte stream mode. The next two bytes must be the starting address such as `0x000a`. The programmer will send an ACK signal to say it is ready for the byte stream. Data can be sent to the programmer in 64 byte chunks. After those 64 bytes have been written the programmer will again send an ACK signal. It does this to buffer data but to let the programming computer know if it encountered any errors during the writing stage.

Byte stream will exit when the address reaches the limit for the chip being programmed.

The programmer can also be sent a handshake when in "waiting" mode. The programmer will respond with an ACK. This can be used to check if it is alive.

### Cancel byte stream

At any point it might be needed to cancel the byte stream mode (to move to another address for example). In this case then the byte stream mode can be cancelled by sending a `0xff` signal. This will cancel the current state and put the programmer back into the initial power on state. If you need to write the value of `0xff` then this must be repeated as the next immediate byte.

## Notes:

As the Pi Pico is 3.3v tolerant it can only read the EEPROM if connected to a 3.3v supply and the EEPROM supports this. If you try to drive the GPIO pins with 5v it could damage the Pico. This is why the EEPROM must be connected to a separate 5v supply when programming as the AT28C256 required a source greater than 3.8v typical according to the datasheet.

This worked for me for the AT28C256. This does not mean it will work for every parallel EEPROM. This is by no means a perfect programmer, it was just a personal project that I felt like making. ***Use at your own risk!***
