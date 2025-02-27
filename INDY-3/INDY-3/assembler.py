# Tool to assemble a text file of assembly instructions into actual bytes to be read by the VM
# Author: Matthew Watson

from enum import Enum

class Instr_class(Enum):
    AND = 0
    OR = 1
    XOR = 2
    BIT_TEST = 3
    ADD = 4
    SUBTRACT = 5
    COMPARE = 6
    INCREMENT = 7
    DECREMENT = 8
    LEFT_SHIFT = 9
    RIGHT_SHIFT = 10
    LEFT_ROTATE = 11
    RIGHT_ROTATE = 12
    LOAD = 13
    STORE = 14
    BRANCH_IF_CLEAR = 15
    BRANCH_IF_SET = 16
    JUMP = 17
    JUMP_TO_SUBROUTINE = 18

class Instr_addr_mode(Enum):
    INVALID = 0
    IMMEDIATE = 1
    RELATIVE = 2
    ZEROPAGE = 3
    ZPX = 9
    ZPY = 10
    ABSOLUTE = 4
    ABX = 11
    ABY = 12
    INDIRECT = 5
    INDIRECT_INDEX_Y = 7
    INDEX_INDIRECT_X = 8

class Instr_reg_operand(Enum):
    NONE = 0
    X = 1
    Y = 2
    
    FLAG_CARRY = 3
    FLAG_OVERFLOW = 4
    FLAG_ZERO = 5
    FLAG_NEGATIVE = 6
    FLAG_INTERRUPT_DISABLE = 7


# global vars
working_bytes = bytearray()

named_addresses = {}

locations_to_replace = {}

PROGRAM_COUNTER_ENTRY_POINT = 0x200

implicits_str = 'nop brk rts rti tax tay txa tya tsx txs pha pla php plp sec clc clv'
implicits = {}
implicits_str = implicits_str.split(' ')
for i, item in enumerate(implicits_str):
    implicits[item] = i

def twos_complement_byte(value:int):
    print(f"twos comp. of {value}")
    return (~value + 1) & 0xff

def get_int(value:str):
    base = 10
    if value[0] == '$':
        value = value[1:]
        base = 16
    result = int(value, base)
    if result < 0:
       result = twos_complement_byte(abs(result))
    return result

def add_numeric_value(value, verify_16_bit=False, verify_8_bit=False):
    if type(value) is str:
        num = get_int(value)
    elif type(value is int):
        num = value
    else:
        print("Error with add_numeric, type is not int or string")
        exit(-1)

    if verify_16_bit:
        # must be two bytes worth of data. Will just take the lowest two bytes if it's more.
        working_bytes.append(num & 0xFF)
        num >>= 8
        working_bytes.append(num & 0xFF)
    elif verify_8_bit:
        # must be one byte worth of data. Will just take the lowest two bytes if it's more.
        working_bytes.append(num & 0xFF)
    else:
        while num > 0:
            # If the number is bigger than a byte, add the numbers a byte at a time.
            working_bytes.append(num & 0xFF)
            num >>= 8
           
            #print(value, num, bytes)

def check_int16_or_str(value):
    num = 0
    is_addr = False
    try:
        num = get_int(value)
    except:
        num = 0
        is_addr = True
        locations_to_replace[len(working_bytes)] = value
    return num, is_addr

def process_line(line:str):
    # Delete any commented out section
    stripped_line = line.strip(' \t\n')
    semicolon_pos = stripped_line.find(';')
    
    if semicolon_pos != -1:
        stripped_line = stripped_line[:semicolon_pos].strip(' \t\n')

    if len(stripped_line) == 0:
        return

    # Add a named address
    if stripped_line[0] == '.':
        named_addresses[stripped_line[1:].strip(' \t\n')] = len(working_bytes) + PROGRAM_COUNTER_ENTRY_POINT
        return
    
    split_line = stripped_line.split(' ')
    instr = split_line[0].lower()

    if instr in implicits_str:
        working_bytes.append(implicits[instr])
        return
    elif len(split_line) == 1:
        match instr:
            case 'inx':
                current_opcode = 0x28
            case 'iny':
                current_opcode = 0x29
            case 'dex':
                current_opcode = 0x2A
            case 'dey':
                current_opcode = 0x2B
            case 'shl':
                current_opcode = 0x2C
            case 'shr':
                current_opcode = 0x2D
            case 'rol':
                current_opcode = 0x2E
            case 'ror':
                current_opcode = 0x2F
            case _:
                print('Error:', instr, "is not an implicit opcode.")
                exit(-1)
        working_bytes.append(current_opcode)
        return
    elif instr == 'dat':
        
        is_in_string = False
        # insert data into the byte array
        for item in split_line[1:]:
            if is_in_string:
                if '"' in item:
                    is_in_string = False
            else:
                match item[0]:
                    case '"':
                        # character string
                        is_in_string = True
                        string = stripped_line[3:].strip(' "')
                        #print(string)
                        for letter in string:
                            working_bytes.append(ord(letter))
                    case _:
                        add_numeric_value(item)
                        
        return
    else:
        right_hand = stripped_line[3:].strip(' \t\n')

        current_opcode = 0x00
        current_number = 0x00

        instr_size = 2
        addr_mode = Instr_addr_mode.INVALID

        is_named_addr = False

        # gather information about addressing mode
        rh_no_paren = right_hand.strip('()')
        if rh_no_paren != right_hand:
            rh_no_paren = rh_no_paren.replace(')', '')
            # indirect addressing modes

            split_rh = rh_no_paren.split(' ')
            if len(split_rh) == 1:
                # standard indirect addressing
                # only one instruction does this
                working_bytes.append(0x89)
                print(split_rh)
                add_numeric_value(split_rh[0], True)
                return
            else:
                check = check_int16_or_str(split_rh[0])
                current_number = check[0]
                is_named_addr = check[1]

                instr_size = 3
                match split_rh[1].lower():
                    case 'x':
                        # indexed indirect
                        addr_mode = Instr_addr_mode.INDEX_INDIRECT_X
                    case 'y':
                        # indirect indexed
                        addr_mode = Instr_addr_mode.INDIRECT_INDEX_Y
                    case _:
                        # error
                        print("Error with indirect instruction, no register of that name.")
                        exit(-1)
        else:
            value = split_line[1]
            if value[0] == '#':
                # immediate addressing
                num = get_int(value[1:])
                if 0 <= num <= 255:
                    current_number = num
                    addr_mode = Instr_addr_mode.IMMEDIATE
                else:
                    # error
                    print("Error, immediate value too large (must be between 0 and 255 or 0x0 and 0xFF)")
                    exit(-1) 
                
            else:
                check = check_int16_or_str(value)
                current_number = check[0]
                is_named_addr = check[1]

                if 0 <= current_number <= 255 and not is_named_addr:
                    # zero page addressing
                    if len(split_line) == 2:
                        addr_mode = Instr_addr_mode.ZEROPAGE
                    else:
                        if split_line[2].lower() == 'x':
                            addr_mode = Instr_addr_mode.ZPX
                        elif split_line[2].lower() == 'y':
                            addr_mode = Instr_addr_mode.ZPY
                        else:
                            # error
                            print("Error, that register does not exist.")
                            exit(-1)
                elif 256 <= current_number <= 0xFFFF or is_named_addr:
                    instr_size = 3

                    # absolute addressing
                    if len(split_line) == 2:
                        addr_mode = Instr_addr_mode.ABSOLUTE
                    else:
                        if split_line[2].lower() == 'x':
                            addr_mode = Instr_addr_mode.ABX
                        elif split_line[2].lower() == 'y':
                            addr_mode = Instr_addr_mode.ABY
                        else:
                            # error
                            print("Error, that register does not exist. abs")
                            exit(-1)
                else:
                    # error
                    print("Error, number too large")
                    exit(-1)

        # Apply the high bits which signify the addressing mode
        match addr_mode:
            case Instr_addr_mode.ZEROPAGE | Instr_addr_mode.ZPX | Instr_addr_mode.ZPY:
                current_opcode |= 0b0100_0000
            case Instr_addr_mode.ABX | Instr_addr_mode.ABY | Instr_addr_mode.INDEX_INDIRECT_X | Instr_addr_mode.INDIRECT_INDEX_Y:
                current_opcode |= 0b1000_0000
            case Instr_addr_mode.IMMEDIATE:
                current_opcode |= 0b1100_0000
            case Instr_addr_mode.ABSOLUTE:
                current_opcode |= 0b1110_0000
            case _:
                # error
                print("Error, addressing mode", addr_mode, "not supported.")
                exit(-1)
        
        # Handle odd instructions out
        is_normal = True

        if Instr_addr_mode == Instr_addr_mode.INDIRECT_INDEX_Y:
            is_normal = False
            match instr:
                case 'and':
                    current_opcode |= 0b1000
                case 'ora':
                    current_opcode |= 0b1001
                case 'xor':
                    current_opcode |= 0b1010
                case 'lda':
                    current_opcode |= 0b1011
                case 'adc':
                    current_opcode |= 0b1100
                case 'sbc':
                    current_opcode |= 0b1101
                case 'cmp':
                    current_opcode |= 0b1110
                case 'sta':
                    current_opcode |= 0b1111
                case _:
                    # error
                    print("No indirect index y instruction", instr)
                    exit(-1)
        
        # Absolute jumps and misplaced lda/sta
        match instr:
            case 'jmp':
                is_normal = False
                current_opcode = 0xE9
            case 'jsr':
                is_normal = False
                current_opcode = 0xEB
            case 'lda':
                if addr_mode == Instr_addr_mode.ABY:
                    current_opcode = 0x93
                    is_normal = False
                elif addr_mode == Instr_addr_mode.INDEX_INDIRECT_X:
                    current_opcode = 0xA3
                    is_normal = False
            case 'sta':
                if addr_mode == Instr_addr_mode.ABY:
                    current_opcode = 0xB7
                    is_normal = False
                elif addr_mode == Instr_addr_mode.INDEX_INDIRECT_X:
                    current_opcode = 0xA7
                    is_normal = False
        
        if addr_mode == Instr_addr_mode.ABY:
            # out of place math stuff

            match instr:
                case 'and':
                    is_normal = False
                    current_opcode = 0xB8
                case 'ora':
                    is_normal = False
                    current_opcode = 0xB9
                case 'xor':
                    is_normal = False
                    current_opcode = 0xBA
                case 'adc':
                    is_normal = False
                    current_opcode = 0xBC
                case 'sbc':
                    is_normal = False
                    current_opcode = 0xBD
                case 'cmp':
                    is_normal = False
                    current_opcode = 0xBE
        
        elif addr_mode in [Instr_addr_mode.INDEX_INDIRECT_X, Instr_addr_mode.ZPX, Instr_addr_mode.ZPY]:
            current_opcode |= 0b10_0000

        if is_normal:
            match instr:
                case 'and':
                    current_opcode |= 0b0_0000
                case 'ora':
                    current_opcode |= 0b0_0001
                case 'xor':
                    current_opcode |= 0b0_0010
                case 'bit':
                    current_opcode |= 0b0_0011
                case 'adc':
                    current_opcode |= 0b0_0100
                case 'sbc':
                    current_opcode |= 0b0_0101
                case 'cmp':
                    current_opcode |= 0b0_0110
                case 'cpx':
                    current_opcode |= 0b0_0111
                case 'inx' | 'inc':
                    current_opcode |= 0b0_1000
                case 'iny':
                    current_opcode |= 0b0_1001
                case 'dex' | 'dec':
                    current_opcode |= 0b0_1010
                case 'dey':
                    current_opcode |= 0b0_1011
                case 'shl':
                    current_opcode |= 0b0_1100
                case 'shr':
                    current_opcode |= 0b0_1101
                case 'rol':
                    current_opcode |= 0b0_1110
                case 'ror':
                    current_opcode |= 0b0_1111
                # loads/stores (and cpy)
                case 'lda':
                    current_opcode |= 0b1_0000
                case 'ldx':
                    current_opcode |= 0b1_0001
                case 'ldy':
                    current_opcode |= 0b1_0010
                case 'cpy':
                    current_opcode |= 0b1_0011
                case 'sta':
                    current_opcode |= 0b1_0100
                case 'stx':
                    current_opcode |= 0b1_0101
                case 'sty':
                    current_opcode |= 0b1_0110
                # branches
                case 'bcc':
                    current_opcode |= 0b1_1000
                case 'bcs':
                    current_opcode |= 0b1_1001
                case 'bne':
                    current_opcode |= 0b1_1010
                case 'beq':
                    current_opcode |= 0b1_1011
                case 'bpl':
                    current_opcode |= 0b1_1100
                case 'bmi':
                    current_opcode |= 0b1_1101
                case 'bvc':
                    current_opcode |= 0b1_1110
                case 'bvs':
                    current_opcode |= 0b1_1111
                case _:
                    print("No normal instruction", instr)
                    exit(-1)
                
        # apply everything
        working_bytes.append(current_opcode)
        add_numeric_value(current_number, instr_size==3 or is_named_addr, instr_size==2 and not is_named_addr)

        print(stripped_line, addr_mode)



def assemble_file(filename:str):
    with open(filename, mode='r') as file:
        for line in file:
            process_line(line)

        for key in locations_to_replace:
            #print(locations_to_replace[key], named_addresses)
            addr = named_addresses[locations_to_replace[key]]
            working_bytes[key+1] = addr & 0xff
            addr >>= 8
            working_bytes[key+2] = addr & 0xff

import sys

argc = len(sys.argv)
argv = sys.argv


if argc <= 1:
    filename = input("Please enter filename: ")
else:
    filename = argv[1]

assemble_file("tests/"+filename)
print(named_addresses)
print(locations_to_replace)

immut_bytes = bytes(working_bytes)
with open(f"outs/{filename.split('.')[0]}.kvmbin", mode='wb') as file:
    file.write(immut_bytes)
