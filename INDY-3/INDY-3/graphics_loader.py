# Tool to load a graphics file
# Author: Matthew Watson

import pygame

# Get the 1-dimensional pixel array from a surface.
def get_pixels(surf:pygame.Surface):
    pixarr = pygame.PixelArray(surf)
    a = [j for sub in pixarr for j in sub]
    flatten = []
    #print(pixarr.shape[0], pixarr.shape[1])
    length = pixarr.shape[0]*pixarr.shape[1]
    for i in range(length):
        '''
        0 : 0
        1 : 16
        2 : 32
        3 : 48
        4 : 1
        5 : 17
        6 : 33
        7 : 49
        8 : 2
        '''
        pos = ((i*pixarr.shape[1])+(i//pixarr.shape[0])) % length
        #print(pos)
        flatten.append(a[pos])
    return flatten

#colors = {0xffffff:0, 0x000000:1, 0x009696:2, 0xffff96:3}
#colors = {0xffffff:0, 0x000000:1, 0xff0000:2, 0x0000ff:3}

colors = {}

# process four pixels
def process_pix(arr, position):
    byte = 0b00000000

    for i in range(4):
        color = arr[position + i]
        if colors.get(color) != None:
            byte = byte | (colors[color] << (i * 2))
        else:
            # This makes it so it will record four colors and then any other ones it sees will wrap around.
            colors[color] = len(colors) % 4

            byte = byte | (colors[color] << (i * 2))
    return byte

def createPixFile(filename:str, outfile_name:str):
    surf = pygame.image.load(filename)#.convert()
    size = surf.get_size()

    # image has to be 16x16 tiles.
    if size[0] != 128 or size[1] != 128:
        print('Image is the wrong size. Must be 128 by 128 pixels.')
        return -1

    filearr = []

    # the method will return a list of the colors in the original surface
    flatten = get_pixels(surf)

    for i in range(256):
        pix_ptr = (i * 8) + (i // 16 * (128*7))
        #pix_ptr = (i * 8) + (i // 16) * 8
        count = 0
        for _ in range(16):
            pattern = process_pix(flatten, pix_ptr+count)
            if pattern == None:
                print("Error with process_pix")
                return -1
            filearr.append(pattern)
            count+=4
            if count > 7:
                pix_ptr += 128
                count = 0

    for item in filearr:
        if not (0 <= item <= 0xff):
            print("overflow!", item)
            return -1
    bytes_list = bytearray(filearr)

    with open(f"graphics/out/{outfile_name}.kvmpix", mode="wb") as bin_file:
        bin_file.write(bytes_list)
    
    return 0

def load_palettes(filename:str, outfile_name:str):
    surf = pygame.image.load(filename)#.convert()
    size = surf.get_size()

    if size[0] != 4 or size[1] != 16:
        print('Image is the wrong size. Must be 4 by 16 pixels.')
        return -1
    
    palettes = []

    flatten = get_pixels(surf)

    # for item in flatten:
    #     print(hex(item), end=" ")
    
    count = -1
    for i in range(64):
        if i % 4 == 0:
            count += 1
            #print(count)

        shift = 16
        #print()
        for _ in range(3):
            #print(hex(flatten[i]), hex((flatten[i] & (0xFF << shift)) >> shift))
            palettes.append((flatten[i] & (0xFF << shift)) >> shift)
            shift -= 8
            
    
    byte_palettes = bytearray(palettes)

    with open(f"graphics/out/{outfile_name}.kvmpal", mode="wb") as bin_file:
        bin_file.write(byte_palettes)
    
    return 0



import sys

argv = sys.argv
argc = len(argv)

#pygame.init()
#resolution = (256,256)
#display_screen = pygame.display.set_mode(resolution)

# Set the filenames based on user input or command line input.
if argc < 3:
    pix_file_name = input("Enter file name for graphics: ")
    palette_file_name = input("Enter file name for palettes: ")
else:
    pix_file_name = argv[1]
    palette_file_name = argv[2]

# If the file name is '-1', just skip it.
if(pix_file_name == "-1"):
    pix_result = 0
else:
    pix_result = createPixFile(f"graphics/{pix_file_name}.bmp", pix_file_name)

if(palette_file_name == "-1"):
    pal_result = 0
else:
    pal_result = load_palettes(f"graphics/{palette_file_name}.bmp", palette_file_name)

if pix_result == 0 and pal_result == 0:
    # We're good
    print("Graphics loaded sucessfully.")
    sys.exit(0)
else:
    # Load failure
    sys.exit(-1)
    

'''
for i in range(64):
    pix_ptr = (i * 8) + (i // 16 * (128*16))
    print(i, pix_ptr)
'''

#print(hex((0xAF1788 & (0xFF << 16))>>16))

'''
Pix file format.
4 pixels per byte, bytes are stored like: [3, 2, 1, 0], [3, 2, 1, 0], etc

palette format
3 bytes per color, 12 per palette
r g b r g b r g b r g b, ...
'''

    

