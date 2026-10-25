"""Minimal reader for GameCube/Wii REL files (relF.rel), enough to resolve calls into main.dol.

Rel(data).sections         -> list of (file offset, size, executable)
Rel(data).relocs(module)   -> iterator of (section, offset in section, type, target section, addend)
                              target section is only meaningful for relocations against the rel itself
Rel(data).dol_relocs()     -> same, against main.dol (addend = absolute address)
"""

import struct

R_PPC_ADDR32 = 1
R_PPC_ADDR16_LO = 4
R_PPC_ADDR16_HA = 6
R_PPC_REL24 = 10
R_DOLPHIN_NOP = 201
R_DOLPHIN_SECTION = 202
R_DOLPHIN_END = 203


class Rel:
    def __init__(self, data: bytes):
        self.data = data
        (self.id, _, _, num_sections, section_info, _, _, self.version, self.bss_size,
         self.rel_offset, self.imp_offset, self.imp_size) = struct.unpack_from(">12I", data, 0)
        self.sections = []
        for i in range(num_sections):
            off, size = struct.unpack_from(">II", data, section_info + 8 * i)
            self.sections.append((off & ~1, size, bool(off & 1)))

    def imports(self):
        for i in range(self.imp_size // 8):
            yield struct.unpack_from(">II", self.data, self.imp_offset + 8 * i)

    def relocs(self, module_id):
        """(section, offset, type, target section, addend) for every relocation against module_id."""
        for mod, off in self.imports():
            if mod != module_id:
                continue
            section, pos = 0, 0
            while True:
                delta, rtype, sec, addend = struct.unpack_from(">HBBI", self.data, off)
                off += 8
                pos += delta
                if rtype == R_DOLPHIN_END:
                    break
                if rtype == R_DOLPHIN_SECTION:
                    section, pos = sec, 0
                    continue
                if rtype == R_DOLPHIN_NOP:
                    continue
                yield section, pos, rtype, sec, addend

    def dol_relocs(self):
        return self.relocs(0)

    def file_offset(self, section, offset):
        return self.sections[section][0] + offset
