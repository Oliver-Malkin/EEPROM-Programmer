import serial

WRITE_BYTE = 0x01
WRITE_BYTE_STREAM = 0x02
HANDSHAKE = 0xaa

def writeByte(conn: serial.Serial, data = 0x00, addr=0x0000):
    conn.write(WRITE_BYTE.to_bytes(1, 'big'))
    conn.write(addr.to_bytes(2, 'big'))
    conn.write(data.to_bytes(1, 'big'))
    print("Data returned: 0x"+ser.read().hex())

with serial.Serial(timeout=1) as ser:
    ser.baudrate = 115200
    ser.port = 'COM13'
    ser.open()
    ser.write(HANDSHAKE.to_bytes(1, 'big'))
    print("Handshake: 0x"+ser.read().hex())
    loop = True
    try:
        while loop:
            writeByte(ser, 0x00, 0x0001)
            writeByte(ser, 0x00, 0x0002)
            writeByte(ser, 0xaa, 0x7fff)
            loop = False
    except KeyboardInterrupt:
        loop = False

    ser.close()