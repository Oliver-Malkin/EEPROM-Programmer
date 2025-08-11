import serial

WRITE_BYTE = 0x01
WRITE_BYTE_STREAM = 0x02
HANDSHAKE = 0xaa
CANCEL = 0xff

def writeByte(conn: serial.Serial, data = 0x00, addr=0x0000) -> bool:
    conn.write(WRITE_BYTE.to_bytes(1, 'big'))
    conn.write(addr.to_bytes(2, 'big'))
    conn.write(data.to_bytes(1, 'big'))

with serial.Serial(timeout=1) as ser:
    ser.baudrate = 115200
    ser.port = 'COM13'
    ser.open()
    ser.write(HANDSHAKE.to_bytes(1, 'big'))
    if "06" == ser.read().hex():
        print("Connected")
    loop = True
    try:
        while loop:
            ser.write(WRITE_BYTE_STREAM.to_bytes(1, 'big'))
            ser.write(0x0000.to_bytes(2, 'big'))

            for x in range(0, 11):
                ser.write(x.to_bytes(1, 'big'))

            ser.write(CANCEL.to_bytes(1, 'big'))
            ser.write(HANDSHAKE.to_bytes(1, 'big'))
            if "06" == ser.read().hex():
                print("Programmed")
            else: 
                print("error")
            loop = False
    except KeyboardInterrupt:
        loop = False

    ser.close()