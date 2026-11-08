import os
import struct
import urllib.request
import zlib
import subprocess

SIZE = 16
BACKGROUND = 0x18E3

CLUBS = [
    ("arsenal", "https://assets.football-logos.cc/logos/england/256x256/arsenal.e5528ede.png"),
    ("manchester_city", "https://assets.football-logos.cc/logos/england/256x256/manchester-city.62f9d1f2.png"),
    ("liverpool", "https://assets.football-logos.cc/logos/england/256x256/liverpool.99c48ae3.png"),
    ("real_madrid", "https://assets.football-logos.cc/logos/spain/256x256/real-madrid.5ce15611.png"),
    ("barcelona", "https://assets.football-logos.cc/logos/spain/256x256/barcelona.481f5fb3.png"),
    ("manchester_united", "https://assets.football-logos.cc/logos/england/256x256/manchester-united.7ab9d343.png"),
]

def rgb565(r, g, b):
    return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)

def decode_png(path):
    data = open(path, "rb").read()
    pos = 8
    width = height = color_type = bit_depth = None
    compressed = b""
    palette = b""
    transparency = b""

    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos+4])[0]
        kind = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+length]
        pos += 12 + length

        if kind == b"IHDR":
            width, height, bit_depth, color_type, comp, filt, interlace = struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8 or interlace != 0:
                raise ValueError("Unsupported PNG format")
        elif kind == b"IDAT":
            compressed += chunk
        elif kind == b"PLTE":
            palette = chunk
        elif kind == b"tRNS":
            transparency = chunk
        elif kind == b"IEND":
            break

    channels = {2: 3, 3: 1, 6: 4}.get(color_type)
    if channels is None:
        raise ValueError("Unsupported PNG color type: " + str(color_type))

    raw = zlib.decompress(compressed)
    stride = width * channels
    rows = []
    offset = 0
    previous = [0] * stride

    for y in range(height):
        filter_type = raw[offset]
        offset += 1
        scan = list(raw[offset:offset+stride])
        offset += stride

        for i in range(stride):
            a = scan[i-channels] if i >= channels else 0
            b = previous[i]
            c = previous[i-channels] if i >= channels else 0

            if filter_type == 1:
                scan[i] = (scan[i] + a) & 255
            elif filter_type == 2:
                scan[i] = (scan[i] + b) & 255
            elif filter_type == 3:
                scan[i] = (scan[i] + ((a+b)//2)) & 255
            elif filter_type == 4:
                p = a+b-c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                predictor = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                scan[i] = (scan[i] + predictor) & 255
            elif filter_type != 0:
                raise ValueError("Unknown PNG filter")

        rows.append(scan)
        previous = scan

    pixels = []
    for row in rows:
        for x in range(width):
            i = x * channels
            if color_type == 6:
                r, g, b, a = row[i:i+4]
            elif color_type == 2:
                r, g, b = row[i:i+3]
                a = 255
            else:
                index = row[i]
                r, g, b = palette[index*3:index*3+3]
                a = transparency[index] if index < len(transparency) else 255

            if a < 255:
                br = ((BACKGROUND >> 11) & 31) * 255 // 31
                bg = ((BACKGROUND >> 5) & 63) * 255 // 63
                bb = (BACKGROUND & 31) * 255 // 31
                r = (r*a + br*(255-a)) // 255
                g = (g*a + bg*(255-a)) // 255
                b = (b*a + bb*(255-a)) // 255

            pixels.append(rgb565(r, g, b))

    return width, height, pixels

def main():
    arrays = []

    for name, url in CLUBS:
        filename = name + ".png"
        resized = name + "_24.png"

        print("Downloading:", name)
        request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            open(filename, "wb").write(response.read())

        subprocess.check_call(["sips", "-z", str(SIZE), str(SIZE), filename, "--out", resized],
                              stdout=subprocess.DEVNULL)

        width, height, pixels = decode_png(resized)

        if width != SIZE or height != SIZE:
            raise ValueError("Wrong image size: " + name)

        values = []
        for i in range(0, len(pixels), 12):
            values.append("  " + ", ".join("0x%04X" % p for p in pixels[i:i+12]))

        arrays.append(
            "const uint16_t logo_%s[%d] PROGMEM = {\n%s\n};\n"
% (name, SIZE * SIZE, ",\n".join(values))
        )

        print("Converted:", name)

    with open("club_logos.h", "w") as f:
        f.write("#ifndef CLUB_LOGOS_H\n#define CLUB_LOGOS_H\n")
        f.write("#include <Arduino.h>\n#include <avr/pgmspace.h>\n\n")
        f.write("\n".join(arrays))
        f.write("\n#endif\n")

    print("\nSUCCESS! club_logos.h created.")

if __name__ == "__main__":
    main()
