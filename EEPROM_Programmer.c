#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#define LED_PIN 25
#define WE 15

#define ADDR_PIN_COUNT 15
#define DATA_PIN_COUNT 8

#define CMD_WRITE_BYTE      0x01    // Write one byte of data. Wait for next instruction
#define CMD_WRITE_STREAM    0x02    // Continually write incomming byte stream, adddresses written to sequentially
#define CMD_CANCEL          0xff    // Cancel current instruction
#define CMD_HANDSHAKE       0xaa    // Handshake command. Will reply with ACK

#define ACK 0x06    // All good. Acknowledged
#define NAK 0x15    // Error. Not acknowldged

typedef enum {
    WAIT_INSTRUCTION,   // Wait for instruction
    WAIT_ADDR_HIGH,     // Wait for addr high byte
    WAIT_ADDR_LOW,      // Wait for addr low byte
    WAIT_DATA,          // Wait for data byte
} states;

void connectionWait()
{
    while (!stdio_usb_connected())
    {
        gpio_put(LED_PIN, 1);
        sleep_ms(200);
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
    }
}

void programByte(uint8_t data, uint16_t addr, const int ADDR_PINS[ADDR_PIN_COUNT], const int DATA_PINS[DATA_PIN_COUNT])
{
    // Set the address
    for (int i = 0; i < ADDR_PIN_COUNT; i++)
    {
        int pin = ADDR_PINS[i]; // Set current address pin
        gpio_put(pin, (addr >> i) & 1);
    }

    // Set the data
    for (int i = 0; i < DATA_PIN_COUNT; i++)
    {
        int pin = DATA_PINS[i]; // Set the current data pin
        gpio_put(pin, (data >> i) & 1);
    }

    // Write - Timings are very forgiving. Could be a lot less than 1us
    sleep_us(1); // Address set up hold time
    gpio_put(WE, 0);
    sleep_us(1); // Write pulse has to be at least 100ns
    gpio_put(WE, 1);
    sleep_us(1); // There is a data hold time of at least 50ns

    // Testing
    /*gpio_put(LED_PIN, 0);
    sleep_ms(100);
    gpio_put(LED_PIN, 1);
    sleep_ms(100);
    */
}

int main()
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, 1);

    // Set up the write enable and make it high. Pull low to write
    gpio_init(WE);
    gpio_set_dir(WE, GPIO_OUT);
    gpio_put(WE, 1);

    connectionWait(); // Wait for a valid CDC connection
                      // Blink LED to show waiting for connection

    int ADDR_PINS[] = { 28, 27, 26, 22, 21, 20, 19, 18, 3, 2, 0, 1, 17, 4, 16 };
    int DATA_PINS[] = { 14, 13, 12, 11, 10, 9, 8, 7 };

    // Set the address pins as output
    for (int i = 0; i < ADDR_PIN_COUNT; i++)
    {
        gpio_init(ADDR_PINS[i]);
        gpio_set_dir(ADDR_PINS[i], GPIO_OUT);
    }

    // Set the data pins as output
    for (int i = 0; i < DATA_PIN_COUNT; i++)
    {
        gpio_init(DATA_PINS[i]);
        gpio_set_dir(DATA_PINS[i], GPIO_OUT);
    }

    uint8_t instruction = 0;    // Current instruction. 0 = nothing
    uint16_t addr = 0;          // Address to be written to
    uint8_t data = 0;           // Data to be written

    states state = WAIT_INSTRUCTION; // Initial state should be wait for an instruction

    while (1)
    {
        uint8_t byte = getchar();

        switch (state) {
        case WAIT_INSTRUCTION: // Wait for the instruction
            if (byte == CMD_WRITE_BYTE) {
                instruction = CMD_WRITE_BYTE;
                state = WAIT_ADDR_HIGH;

            } else if (byte == CMD_WRITE_STREAM) {
                instruction = CMD_WRITE_STREAM;
                state = WAIT_ADDR_HIGH;

            } else if (byte == CMD_HANDSHAKE) {
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
            state = WAIT_DATA;
            break;

        case WAIT_DATA: // Get data, the write to the address
            data = 0; // Reset the data
            data = data | byte;

            programByte(data, addr, ADDR_PINS, DATA_PINS);

            if (instruction == CMD_WRITE_BYTE) {
                sleep_ms(10); // When writing 1 byte there is a write time of 10ms
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
