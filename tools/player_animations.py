#!/usr/bin/env python3
"""Converts a Halo Editing Kit animation graph (.model_animations) into the
file the game loads for a co-op player's Elite (port/linux/game/
player_animations.c):

    python tools/player_animations.py SOURCE.model_animations OUTPUT.antr

The editing kit's tags are big-endian, with each block's elements, each
tag reference's name and each data field's bytes following the structure
that holds them. The game reads tags as they are in a cache file:
little-endian, with blocks and data reached by address. This lays the tag
out that way, with every address as an offset into the file's data, and
lists what the game has to fix up when it loads it:

    'antr', version, data size, relocation count, reference count
    data       the graph, the root structure first (little-endian)
    relocations the offset of each address field in the data; the field
               holds the offset it points to plus one, or 0 for none
    references each tag reference: the field's offset, its group, and the
               tag's name, which the game looks up in the loaded map

Animation frames are swapped value by value: each node's rotation (four
shorts), translation (three floats) and scale (one float), from the frame
data or the default data as the animation's node flags say. Compressed
animations aren't supported (the Elite graph has none).

The structure layouts below come from Invader's tag definitions.
"""

import struct
import sys

MAGIC = b'antr'
VERSION = 1
HEADER_BYTES = 64
FRAME_INFO_FLOATS = {0: 0, 1: 2, 2: 3, 3: 4}

LAYOUTS = {
    'ModelAnimations': (128, [
        (0, 12, 'block', 'ModelAnimationsAnimationGraphObjectOverlay', 'objects'),
        (12, 12, 'block', 'ModelAnimationsAnimationGraphUnitSeat', 'units'),
        (24, 12, 'block', 'ModelAnimationsAnimationGraphWeaponAnimations', 'weapons'),
        (36, 12, 'block', 'ModelAnimationsAnimationGraphVehicleAnimations', 'vehicles'),
        (48, 12, 'block', 'ModelAnimationsDeviceAnimations', 'devices'),
        (60, 12, 'block', 'ModelAnimationsUnitDamageAnimations', 'unit damage'),
        (72, 12, 'block', 'ModelAnimationsAnimationGraphFirstPersonWeaponAnimations', 'first person weapons'),
        (84, 12, 'block', 'ModelAnimationsAnimationGraphSoundReference', 'sound references'),
        (96, 4, 'u32', None, 'limp body node radius'),
        (100, 2, 'u16', None, 'flags'),
        (102, 2, 'bytes', None, ''),
        (104, 12, 'block', 'ModelAnimationsAnimationGraphNode', 'nodes'),
        (116, 12, 'block', 'ModelAnimationsAnimation', 'animations'),
    ]),
    'ModelAnimationsAnimationGraphObjectOverlay': (20, [
        (0, 2, 'u16', None, 'animation'),
        (2, 2, 'u16', None, 'function'),
        (4, 2, 'u16', None, 'function controls'),
        (6, 2, 'bytes', None, ''),
        (8, 12, 'bytes', None, ''),
    ]),
    'ModelAnimationsAnimationGraphUnitSeat': (100, [
        (0, 32, 'bytes', None, 'label'),
        (32, 4, 'u32', None, 'right yaw per frame'),
        (36, 4, 'u32', None, 'left yaw per frame'),
        (40, 2, 'u16', None, 'right frame count'),
        (42, 2, 'u16', None, 'left frame count'),
        (44, 4, 'u32', None, 'down pitch per frame'),
        (48, 4, 'u32', None, 'up pitch per frame'),
        (52, 2, 'u16', None, 'down pitch frame count'),
        (54, 2, 'u16', None, 'up pitch frame count'),
        (56, 8, 'bytes', None, ''),
        (64, 12, 'block', 'ModelAnimationsAnimationWeaponClassAnimation', 'animations'),
        (76, 12, 'block', 'ModelAnimationsAnimationGraphUnitSeatikPoint', 'ik points'),
        (88, 12, 'block', 'ModelAnimationsAnimationGraphWeapon', 'weapons'),
    ]),
    'ModelAnimationsAnimationWeaponClassAnimation': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsAnimationGraphUnitSeatikPoint': (64, [
        (0, 32, 'bytes', None, 'marker'),
        (32, 32, 'bytes', None, 'attach to marker'),
    ]),
    'ModelAnimationsAnimationGraphWeapon': (188, [
        (0, 32, 'bytes', None, 'name'),
        (32, 32, 'bytes', None, 'grip marker'),
        (64, 32, 'bytes', None, 'hand marker'),
        (96, 4, 'u32', None, 'right yaw per frame'),
        (100, 4, 'u32', None, 'left yaw per frame'),
        (104, 2, 'u16', None, 'right frame count'),
        (106, 2, 'u16', None, 'left frame count'),
        (108, 4, 'u32', None, 'down pitch per frame'),
        (112, 4, 'u32', None, 'up pitch per frame'),
        (116, 2, 'u16', None, 'down pitch frame count'),
        (118, 2, 'u16', None, 'up pitch frame count'),
        (120, 32, 'bytes', None, ''),
        (152, 12, 'block', 'ModelAnimationsAnimationWeaponClassAnimation', 'animations'),
        (164, 12, 'block', 'ModelAnimationsAnimationGraphUnitSeatikPoint', 'ik point'),
        (176, 12, 'block', 'ModelAnimationsAnimationGraphWeaponType', 'weapon types'),
    ]),
    'ModelAnimationsAnimationGraphWeaponType': (60, [
        (0, 32, 'bytes', None, 'label'),
        (32, 16, 'bytes', None, ''),
        (48, 12, 'block', 'ModelAnimationsAnimationWeaponTypeAnimation', 'animations'),
    ]),
    'ModelAnimationsAnimationWeaponTypeAnimation': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsAnimationGraphWeaponAnimations': (28, [
        (0, 16, 'bytes', None, ''),
        (16, 12, 'block', 'ModelAnimationsWeaponAnimation', 'animations'),
    ]),
    'ModelAnimationsWeaponAnimation': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsAnimationGraphVehicleAnimations': (116, [
        (0, 4, 'u32', None, 'right yaw per frame'),
        (4, 4, 'u32', None, 'left yaw per frame'),
        (8, 2, 'u16', None, 'right frame count'),
        (10, 2, 'u16', None, 'left frame count'),
        (12, 4, 'u32', None, 'down pitch per frame'),
        (16, 4, 'u32', None, 'up pitch per frame'),
        (20, 2, 'u16', None, 'down pitch frame count'),
        (22, 2, 'u16', None, 'up pitch frame count'),
        (24, 68, 'bytes', None, ''),
        (92, 12, 'block', 'ModelAnimationsVehicleAnimation', 'animations'),
        (104, 12, 'block', 'ModelAnimationSuspensionAnimation', 'suspension animations'),
    ]),
    'ModelAnimationsVehicleAnimation': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationSuspensionAnimation': (20, [
        (0, 2, 'u16', None, 'mass point index'),
        (2, 2, 'u16', None, 'animation'),
        (4, 4, 'u32', None, 'full extension ground depth'),
        (8, 4, 'u32', None, 'full compression ground depth'),
        (12, 8, 'bytes', None, ''),
    ]),
    'ModelAnimationsDeviceAnimations': (96, [
        (0, 84, 'bytes', None, ''),
        (84, 12, 'block', 'ModelAnimationsDeviceAnimation', 'animations'),
    ]),
    'ModelAnimationsDeviceAnimation': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsUnitDamageAnimations': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsAnimationGraphFirstPersonWeaponAnimations': (28, [
        (0, 16, 'bytes', None, ''),
        (16, 12, 'block', 'ModelAnimationsFirstPersonWeapon', 'animations'),
    ]),
    'ModelAnimationsFirstPersonWeapon': (2, [
        (0, 2, 'u16', None, 'animation'),
    ]),
    'ModelAnimationsAnimationGraphSoundReference': (20, [
        (0, 16, 'reference', None, 'sound'),
        (16, 4, 'bytes', None, ''),
    ]),
    'ModelAnimationsAnimationGraphNode': (64, [
        (0, 32, 'bytes', None, 'name'),
        (32, 2, 'u16', None, 'next sibling node index'),
        (34, 2, 'u16', None, 'first child node index'),
        (36, 2, 'u16', None, 'parent node index'),
        (38, 2, 'bytes', None, ''),
        (40, 4, 'u32', None, 'node joint flags'),
        (44, 12, 'u32', None, 'base vector'),
        (56, 4, 'u32', None, 'vector range'),
        (60, 4, 'bytes', None, ''),
    ]),
    'ModelAnimationsAnimation': (180, [
        (0, 32, 'bytes', None, 'name'),
        (32, 2, 'u16', None, 'type'),
        (34, 2, 'u16', None, 'frame count'),
        (36, 2, 'u16', None, 'frame size'),
        (38, 2, 'u16', None, 'frame info type'),
        (40, 4, 'u32', None, 'node list checksum'),
        (44, 2, 'u16', None, 'node count'),
        (46, 2, 'u16', None, 'loop frame index'),
        (48, 4, 'u32', None, 'weight'),
        (52, 2, 'u16', None, 'key frame index'),
        (54, 2, 'u16', None, 'second key frame index'),
        (56, 2, 'u16', None, 'next animation'),
        (58, 2, 'u16', None, 'flags'),
        (60, 2, 'u16', None, 'sound'),
        (62, 2, 'u16', None, 'sound frame index'),
        (64, 1, 'bytes', None, 'left foot frame index'),
        (65, 1, 'bytes', None, 'right foot frame index'),
        (66, 2, 'u16', None, 'main animation index'),
        (68, 4, 'u32', None, 'relative weight'),
        (72, 20, 'data', None, 'frame info'),
        (92, 8, 'u32', None, 'node transform flag data'),
        (100, 8, 'bytes', None, ''),
        (108, 8, 'u32', None, 'node rotation flag data'),
        (116, 8, 'bytes', None, ''),
        (124, 8, 'u32', None, 'node scale flag data'),
        (132, 4, 'bytes', None, ''),
        (136, 4, 'u32', None, 'offset to compressed data'),
        (140, 20, 'data', None, 'default data'),
        (160, 20, 'data', None, 'frame data'),
    ]),
}


class Writer:
    """The converted data, built front to back."""

    def __init__(self):
        self.data = bytearray()
        self.relocations = []
        self.references = []

    def reserve(self, size):
        while len(self.data) % 4:
            self.data.append(0)
        at = len(self.data)
        self.data.extend(bytes(size))
        return at

    def address(self, field_offset, target):
        """an address field pointing at target (None: none)"""
        struct.pack_into('<I', self.data, field_offset, 0 if target is None else target + 1)
        self.relocations.append(field_offset)


class Converter:
    def __init__(self, source):
        if source[0x24:0x28] != MAGIC or source[0x3C:0x40] != b'blam':
            raise SystemExit('not an editing kit animation graph')
        self.source = source
        self.at = HEADER_BYTES
        self.out = Writer()

    def take(self, size):
        chunk = self.source[self.at:self.at + size]
        if len(chunk) != size:
            raise SystemExit('the tag ends early')
        self.at += size
        return chunk

    def elements(self, name, count):
        """converts count elements of a structure, then their children;
        returns the offset of the first"""
        size, fields = LAYOUTS[name]
        first = self.out.reserve(size * count)
        raws = [self.take(size) for _ in range(count)]
        for index, raw in enumerate(raws):
            self.swap(fields, raw, first + index * size)
        for index, raw in enumerate(raws):
            self.children(name, fields, raw, first + index * size)
        return first

    def swap(self, fields, raw, at):
        """the element's plain fields, little-endian"""
        data = self.out.data
        for offset, size, kind, _, _ in fields:
            if kind == 'bytes':
                data[at + offset:at + offset + size] = raw[offset:offset + size]
            elif kind == 'u16':
                for part in range(0, size, 2):
                    data[at + offset + part:at + offset + part + 2] = raw[offset + part:offset + part + 2][::-1]
            elif kind == 'u32':
                for part in range(0, size, 4):
                    data[at + offset + part:at + offset + part + 4] = raw[offset + part:offset + part + 4][::-1]

    def children(self, name, fields, raw, at):
        """the element's blocks, references and data, in the file's order"""
        for offset, _, kind, sub, field_name in fields:
            if kind == 'block':
                count = struct.unpack_from('>i', raw, offset)[0]
                struct.pack_into('<iII', self.out.data, at + offset, count, 0, 0)
                self.out.address(at + offset + 4, self.elements(sub, count) if count > 0 else None)
            elif kind == 'reference':
                group, _, length, _ = struct.unpack_from('>IIiI', raw, offset)
                tag_name = self.take(length + 1)[:-1].decode('latin-1') if length > 0 else ''
                struct.pack_into('<IIiI', self.out.data, at + offset, group, 0, len(tag_name), 0xFFFFFFFF)
                if tag_name:
                    self.out.references.append((at + offset, group, tag_name))
            elif kind == 'data':
                size = struct.unpack_from('>i', raw, offset)[0]
                content = self.take(size)
                if name == 'ModelAnimationsAnimation':
                    content = animation_data(field_name, raw, content)
                target = self.out.reserve(size) if size > 0 else None
                if target is not None:
                    self.out.data[target:target + size] = content
                struct.pack_into('<iIiII', self.out.data, at + offset, size, 0, 0, 0, 0)
                self.out.address(at + offset + 12, target)

    def file(self):
        root = self.elements('ModelAnimations', 1)
        if root != 0 or self.at != len(self.source):
            raise SystemExit('the tag is not the size its fields say')
        permutations_prepare(self.out.data)
        out = self.out
        parts = [struct.pack('<4sIIII', MAGIC, VERSION, len(out.data), len(out.relocations), len(out.references)),
                 bytes(out.data), struct.pack('<%dI' % len(out.relocations), *out.relocations)]
        for field_offset, group, tag_name in out.references:
            name = tag_name.encode('latin-1') + b'\0'
            name += bytes(-len(name) % 4)
            parts.append(struct.pack('<III', field_offset, group, len(name)) + name)
        return b''.join(parts)


ANIMATION_BYTES = 180
ANIMATIONS_BLOCK = 116


def permutations_prepare(data):
    """Fills in what the tools compute as they build a map: an animation's
    permutations are a chain by 'next animation', and the game picks one by
    walking the chain to the first whose running share of the weights covers
    a random number. Each animation gets the chain's first as its parent and
    its running share as its normalized weight."""
    count, address, _ = struct.unpack_from('<iII', data, ANIMATIONS_BLOCK)
    first = address - 1

    def field(index, offset, fmt):
        return struct.unpack_from(fmt, data, first + index * ANIMATION_BYTES + offset)[0]

    nexts = [field(index, 56, '<h') for index in range(count)]
    followed = {next_index for next_index in nexts if 0 <= next_index < count}
    for base in range(count):
        if base in followed:
            continue
        chain = []
        index = base
        while 0 <= index < count and index not in chain:
            chain.append(index)
            index = nexts[index]
        weights = [field(index, 48, '<f') or 1.0 for index in chain]
        total = sum(weights)
        running = 0.0
        for index, weight in zip(chain, weights):
            running += weight
            at = first + index * ANIMATION_BYTES
            struct.pack_into('<h', data, at + 66, base)
            struct.pack_into('<f', data, at + 68, 1.0 if index == chain[-1] else running / total)


def node_flags(raw, offset, count):
    """the animation's per-node flags (two big-endian words) as one number"""
    low, high = struct.unpack_from('>II', raw, offset)
    return [bool(((high << 32) | low) >> node & 1) for node in range(count)]


def animation_data(field_name, raw, content):
    """an animation's frame info, default data or frame data, swapped"""
    frame_count, frame_size, frame_info_type = struct.unpack_from('>HHH', raw, 34)
    node_count = struct.unpack_from('>H', raw, 44)[0]
    if struct.unpack_from('>H', raw, 58)[0] & 1:
        raise SystemExit('compressed animations are not supported')
    if field_name == 'frame info':
        return swap_words(content, len(content))
    translations = node_flags(raw, 92, node_count)
    rotations = node_flags(raw, 108, node_count)
    scales = node_flags(raw, 124, node_count)
    if field_name == 'default data':
        components = [(not rotations[node], not translations[node], not scales[node]) for node in range(node_count)]
        frames = 1
    else:
        components = [(rotations[node], translations[node], scales[node]) for node in range(node_count)]
        frames = frame_count
    out = bytearray()
    at = 0
    for _ in range(frames):
        for rotation, translation, scale in components:
            if rotation:
                out += b''.join(content[at + part:at + part + 2][::-1] for part in range(0, 8, 2))
                at += 8
            if translation:
                out += swap_words(content[at:at + 12], 12)
                at += 12
            if scale:
                out += content[at:at + 4][::-1]
                at += 4
    if at != len(content):
        raise SystemExit("an animation's %s is not the size its nodes say" % field_name)
    return bytes(out)


def swap_words(content, size):
    return b''.join(content[part:part + 4][::-1] for part in range(0, size, 4))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    with open(sys.argv[1], 'rb') as source:
        converted = Converter(source.read()).file()
    with open(sys.argv[2], 'wb') as output:
        output.write(converted)
    print('wrote %s (%d bytes)' % (sys.argv[2], len(converted)))


if __name__ == '__main__':
    main()
