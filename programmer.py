import serial

WRITE_BYTE = 0x01
WRITE_BYTE_STREAM = 0x02
READ_BYTE = 0x03
READ_BYTE_STREAM = 0x04
HANDSHAKE = 0xaa
CANCEL = 0xff

# The AT28C256 has a page size of 64 bytes and has a 15 bit address 
PAGE_SIZE = 64
ADDR_SIZE = 2**15
TOTAL_PAGES = ADDR_SIZE/PAGE_SIZE

# Write 1 byte of data at a given address
def writeByte(conn: serial.Serial, addr=0x0000, data = 0x00) -> bool:
    print(f"Writing {hex(data)} to address {hex(addr)}")
    conn.write(WRITE_BYTE.to_bytes(1, 'big'))
    conn.write(addr.to_bytes(2, 'big'))
    conn.write(data.to_bytes(1, 'big'))
    return("06" == conn.read().hex())

# This does not check if there is a 0xFF value in the data! It simply spits out the data it has in the data variable
# Use pageWriter for writing data to the EEPROM in bulk
def writeByteStream(conn: serial.Serial, addr=0x0000, data = [0x00]) -> bool:
    conn.write(WRITE_BYTE_STREAM.to_bytes(1, 'big'))
    conn.write(addr.to_bytes(2, 'big'))
    conn.write(bytearray(data)) # Stream the data to the device

    # Cancel the stream
    conn.write(CANCEL.to_bytes(1, 'big'))
    conn.write(HANDSHAKE.to_bytes(1, 'big'))
    return("06" == conn.read().hex())

# Read 1 byte from address
def readByte(conn: serial.Serial, addr=0x0000) -> str:
    if (input("CHECK YOUR VOLTAGE. MUST BE 3.3v. BEGIN READ? (Y/n) ").lower() == 'y'):
        print(f"Reading byte from address {hex(addr)}")
        conn.write(READ_BYTE.to_bytes(1, 'big'))
        conn.write(addr.to_bytes(2, 'big'))
        return(conn.read().hex())
    return("err")

# Reads from start address to end address
def readByteStream(conn: serial.Serial, addrStart = 0x0000, addrEnd = 0x0000) -> list:
    if (input("CHECK YOUR VOLTAGE. MUST BE 3.3v. BEGIN READ? (Y/n) ").lower() == 'y'): 
        print(f"Reading from addresses {hex(addrStart)} to {hex(addrEnd)}")   
        data = []
        conn.write(READ_BYTE_STREAM.to_bytes(1, 'big'))
        conn.write(addrStart.to_bytes(2, 'big'))
        conn.write(addrEnd.to_bytes(2, 'big'))

        for x in range(addrStart, addrEnd+1):
            data.append(conn.read().hex())

        return(data)
    else:
        return(['err'])

# Provide an address and it will return a dict for the start/end addresses and page number
def getPageLimits(addr) -> dict[int, int, int]:
    page = addr // PAGE_SIZE
    startAddress = PAGE_SIZE*page
    endAddress = startAddress+(PAGE_SIZE-1)

    return({"start": startAddress, "end": endAddress, "page": page})

# Repalce all values of 0xff with 0xff 0xff
def replace_ff(data: list[int]) -> list[int]:
    if (0xff not in data):
        return data
    result = []
    for val in data:
        result.append(val)
        if val == 0xFF:
            result.append(val)  # repeat it
    return result

# Write the datastream
def pageWriter(conn: serial.Serial, startAddr: int, data: list):
    endAddr = startAddr+len(data)-1
    startPageInfo = getPageLimits(startAddr)
    endPageInfo = getPageLimits(endAddr)
    totalPages = endPageInfo['page'] - startPageInfo['page']

    addr = startAddr # current address counter
    dataIndex = 0
    for i in range(totalPages+1):
        toWrite = []
        for j in range(addr, getPageLimits(addr)['end']+1):
            try:
                toWrite.append(data[dataIndex])
                dataIndex+=1
            except IndexError:
                break

        if (writeByteStream(conn, addr, replace_ff(toWrite))):
            print(f"Following data written from address {hex(addr)}\n{toWrite}\n")
        else:
            print(f"Error occured while writing from address {hex(addr)}\n{toWrite}\n")
        addr = startAddr+dataIndex


with serial.Serial(timeout=1) as ser:
    ser.baudrate = 115200
    ser.port = 'COM13'
    ser.open()
    ser.write(HANDSHAKE.to_bytes(1, 'big'))

    print("Connecting to programmer...")
    if "06" == ser.read().hex():
        print("Connected")
        # Programmer code here #
    
        data = [0xae]*8     # Programming some random data
        startAddr = 0x0fe6  # Start programming from this address

        pageWriter(ser, startAddr, data) # Tell the programmer to write that data
        print(readByteStream(ser, startAddr, startAddr+len(data)-1)) # Read the data we've just programmed

        print("Finished")

    else:
        print("Error")

    ser.close()