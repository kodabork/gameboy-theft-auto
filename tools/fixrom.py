# Fills in the GBA cartridge header: title, game code, fixed byte, complement check,
# and the boot logo, which real hardware requires (the BIOS refuses to start a cartridge
# whose logo bytes are wrong; emulators such as mGBA boot without it). The logo is the
# standard 156-byte table every homebrew toolchain writes (devkitPro gbafix, the Rust
# gbafix crate, GBATEK); a ROM you own can be given instead as the second argument.
import sys
path = sys.argv[1]
logo_src = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
d = bytearray(open(path, 'rb').read())
LOGO = bytes.fromhex(
    '24FFAE51699AA2213D84820A84E409AD11248B98C0817F21A352BE199309CE20'
    '10464A4AF82731EC58C7E83382E3CEBF85F4DF94CE4B09C194568AC01372A7FC'
    '9F844D73A3CA9A615897A327FC039876231DC7610304AE56BF38840040A70EFD'
    'FF52FE036F9530F197FBC08560D68025A963BE03014E38E2F9A234FFBB3E0344'
    '780090CB88113A9465C07C6387F03CAFD625E48B380AAC7221D4F807')
assert len(LOGO) == 156
d[0x04:0xA0] = open(logo_src, 'rb').read()[0x04:0xA0] if logo_src else LOGO
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
