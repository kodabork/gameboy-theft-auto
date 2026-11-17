# Fills in the GBA cartridge header: title, game code, fixed byte, complement check,
# and (optionally) the boot logo copied from another GBA ROM you own, which real
# hardware requires. Emulators such as mGBA boot without it.
import sys
path = sys.argv[1]
logo_src = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
d = bytearray(open(path, 'rb').read())
if logo_src:
    d[0x04:0xA0] = open(logo_src, 'rb').read()[0x04:0xA0]
d[0xA0:0xAC] = b'GBTHEFTAUTO\0'
d[0xAC:0xB0] = b'SHRB'
d[0xB0:0xB2] = b'00'
d[0xB2] = 0x96
d[0xB3:0xBD] = bytes(10)
chk = (-(sum(d[0xA0:0xBD]) + 0x19)) & 0xFF
d[0xBD] = chk
while len(d) % 4: d.append(0)
open(path, 'wb').write(d)
print('header ok, checksum %02X, %d bytes' % (chk, len(d)))
