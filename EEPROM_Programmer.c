#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

// "Special" pins
#define LED_PIN 25  // Built in LED pin
#define WE 15       // Write enable (active low)
#define OE 6        // Output enable (active low)

// Responce codes
#define ACK 0x06    // Acknowledged
#define NAK 0x15    // Not acknowldged

// Commands
#define CMD_WRITE_BYTE          0x01 // Write one byte of data. Wait for next instruction
#define CMD_WRITE_STREAM        0x02 // Continually write incomming byte stream, adddresses written to sequentially
#define CMD_READ_BYTE           0x03 // Read a byte of data from the EEPROM
#define CMD_READ_BYTE_STREAM    0x04 // Read a byte stream from address 1 to n
#define CMD_HANDSHAKE           0xaa // Handshake command. Will reply with ACK
#define CMD_CANCEL              0xff // Cancel current instruction

// States
typedef enum {
    WAIT_INSTRUCTION,   // Wait for instruction
    WAIT_ADDR_HIGH,     // Wait for addr high byte
    WAIT_ADDR_LOW,      // Wait for addr low byte
    WAIT_DATA,          // Wait for data byte
    READ_DATA,          // Read the data from the EEPROM
} states;

// Pin setup
static const int ADDR_PINS[] = { 28, 27, 26, 22, 21, 20, 19, 18, 3, 2, 0, 1, 17, 4, 16 };
static const int DATA_PINS[] = { 14, 13, 12, 11, 10, 9, 8, 7 };
static const int ADDR_PIN_COUNT = sizeof(ADDR_PINS) / sizeof(int);
static const int DATA_PIN_COUNT = sizeof(DATA_PINS) / sizeof(int);

void connectionWait() {
    while (!stdio_usb_connected()) {
        gpio_put(LED_PIN, 1);
        sleep_ms(200);
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
    }
}

void setDataDir(bool dir) {
    // Set the data pin direction
    for (int i = 0; i < DATA_PIN_COUNT; i++) {
        gpio_init(DATA_PINS[i]);
        gpio_set_dir(DATA_PINS[i], dir);
    }
}

void setAddress(uint16_t addr) {
    // Set the address value out
    for (int i = 0; i < ADDR_PIN_COUNT; i++) {
        int pin = ADDR_PINS[i];
        gpio_put(pin, (addr >> i) & 1);
    }
}

void setData(uint8_t data) {
    // Set the data value out
    for (int i = 0; i < DATA_PIN_COUNT; i++) {
        int pin = DATA_PINS[i];
        gpio_put(pin, (data >> i) & 1);
    }
}

// Address must already be set
uint8_t getData() {
    uint8_t data = 0;
    // Start to read the data into the buffer
    for (int i = 0; i < DATA_PIN_COUNT; i++) {
        data = data | ((gpio_get(DATA_PINS[i]) != 0) << i);
    }

    return(data);
}

// NOTE: The EEPROM must be powered with a 3.3v supply! 
// If powered with 5v the Pi could fry. The programming software should warn before reading, the pico does not check!
uint8_t readByte(uint16_t addr) {
    uint8_t data; // Used to store the data value
    setDataDir(GPIO_IN); // Set the data for input
    setAddress(addr); // Set the address
    gpio_put(OE, 0); // EEPROM output
    sleep_us(1); // More than enough time for the EEPROM to output the data
    data = getData();
    gpio_put(OE, 1); // Stop EEPROM outputting
    setDataDir(GPIO_OUT);
    return(data);
}

void readDataStream(uint16_t addrStart, uint16_t addrEnd) {
    setDataDir(GPIO_IN); // Set the data for input
    gpio_put(OE, 0); // EEPROM output

    for (int addr = addrStart; addr < addrEnd + 1; addr++) {
        setAddress(addr);
        sleep_us(1);
        putchar(getData()); // Output each byte
    }

    gpio_put(OE, 1);
    setDataDir(GPIO_OUT);
}

void programByte(uint8_t data, uint16_t addr) {
    setAddress(addr);
    setData(data);

    // Write - Timings are very forgiving. Could be a lot less than 1us
    // Must be less than 150us though as the chip will start writing
    gpio_put(WE, 0);
    sleep_us(1); // Write pulse has to be at least 100ns
    gpio_put(WE, 1);
    sleep_us(1); // There is a data hold time of at least 50ns
}

void init() {
    stdio_init_all();

    // Set LED pint to putput
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Set up the write enable and make it high. Pull low to write
    gpio_init(WE);
    gpio_set_dir(WE, GPIO_OUT);
    gpio_put(WE, 1);

    // Set up the output enable and make it high. Pull low to output
    // Must be set high at powerup. Could break Pico if EEPROM is powered with 5v
    gpio_init(OE);
    gpio_set_dir(OE, GPIO_OUT);
    gpio_put(OE, 1);

    // Set the address pins as output. They will always be output
    for (int i = 0; i < ADDR_PIN_COUNT; i++) {
        gpio_init(ADDR_PINS[i]);
        gpio_set_dir(ADDR_PINS[i], GPIO_OUT);
    }

    // Initialise the data pins. Pin dir can change for read/write
    for (int i = 0; i < DATA_PIN_COUNT; i++) {
        gpio_init(DATA_PINS[i]);
    }

    setDataDir(GPIO_OUT); // Set as output to start with

    connectionWait(); // Wait for a valid CDC connection
                      // Blink LED to show waiting for connection
}

int main() {
    init();

    uint8_t instruction;    // Current instruction
    uint16_t addr;          // Address to read/write from
    int addrStart = -1;     // Only used when reading byte stream
    uint8_t data;           // Data for read or write

    states state = WAIT_INSTRUCTION; // Initial state should be wait for an instruction

    while (1) {
        uint8_t byte = getchar(); // Read byte in the buffer

        switch (state) {
        case WAIT_INSTRUCTION: // Wait for the instruction
            // All of these need to get the address
            if (byte == CMD_WRITE_BYTE ||
                byte == CMD_WRITE_STREAM ||
                byte == CMD_READ_BYTE ||
                byte == CMD_READ_BYTE_STREAM)
            {
                instruction = byte;
                state = WAIT_ADDR_HIGH;

            } else if (byte == CMD_HANDSHAKE || byte == CMD_CANCEL) {
                putchar(ACK); // Basically do nothing but say I'm alive

            } else {
                putchar(NAK); // What did you send, because I did not understand???
            }
            break;

        case WAIT_ADDR_HIGH: // Get high address byte
            addr = 0; // Reset the address
            addr = addr | byte << 8; // Put the first byte into the upper half
            state = WAIT_ADDR_LOW;
            break;

        case WAIT_ADDR_LOW: // Get low address byte
            addr = addr | byte; // Put the lower half into the addr

            // Next state is dependant on instruction
            if (instruction == CMD_WRITE_BYTE || instruction == CMD_WRITE_STREAM) {
                state = WAIT_DATA;
                break;
            } else if (instruction == CMD_READ_BYTE) {
                state = READ_DATA;
            } else if (instruction == CMD_READ_BYTE_STREAM) {
                if (addrStart == -1) {
                    addrStart = addr; // Store the start address
                    state = WAIT_ADDR_HIGH;
                } else {
                    readDataStream(addrStart, addr);
                    addrStart = -1;
                    state = WAIT_INSTRUCTION;
                }
                break;
            } else {
                state = WAIT_INSTRUCTION;
                break;
            }

        case READ_DATA: // Read the data from the EEPROM
            putchar(readByte(addr));
            state = WAIT_INSTRUCTION;
            break;

        case WAIT_DATA: // Get data, the write to the address
            data = 0; // Reset the data
            data = data | byte;

            programByte(data, addr);

            if (instruction == CMD_WRITE_BYTE) {
                sleep_ms(10); // The internal write cycle takes ~10ms
                putchar(ACK); // Acknowledge end of instruction
                state = WAIT_INSTRUCTION;
            } else if (instruction == CMD_WRITE_STREAM) {
                state = WAIT_DATA; // Only expecting data from now
            } else {
                state = WAIT_INSTRUCTION; // Just so the program doesn't get stuck in case something odd happened
            }
            break;
        }
    }
}