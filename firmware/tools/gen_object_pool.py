#!/usr/bin/env python3
"""Builds the ISOBUS object pool (the VT screen) and the header with its object IDs.

Writes two files that must stay in sync:
  main/isobus/object_pool.iop      binary pool embedded into the firmware
  main/isobus/object_pool_ids.hpp  object IDs used by vt_app.cpp

Run from the firmware directory:  python tools/gen_object_pool.py

Binary layouts follow the parser in AgIsoStack's isobus_virtual_terminal_working_set_base.cpp.
"""
import pathlib
import struct

FIRMWARE_DIR = pathlib.Path(__file__).resolve().parent.parent
ISOBUS_DIR = FIRMWARE_DIR / "main" / "isobus"

NULL_ID = 0xFFFF
STATUS_LINE_LENGTH = 20  # VT strings keep their length, so updates are padded to this

# Colour indices of the standard VT palette
BLACK = 0
WHITE = 1
SILVER = 7

# VT object types
WORKING_SET = 0
DATA_MASK = 1
SOFT_KEY_MASK = 4
KEY = 5
OUTPUT_STRING = 11
FONT_ATTRIBUTES = 23
AUX_FUNCTION_TYPE2 = 31

# VT font size enum
FONT_8X12 = 2
FONT_16X24 = 5
FONT_24X32 = 6

FUNCTION_BOOLEAN_MOMENTARY = 2

WORKING_SET_ID = 0
DATA_MASK_ID = 1
SOFT_KEY_MASK_ID = 2
TITLE_ID = 40
FONT_BIG_ID = 50
FONT_MEDIUM_ID = 51
FONT_SMALL_ID = 52

# Status lines shown on the data mask: (C++ name, object ID, initial text)
STATUS_LINES = [
    ("kTextSection", 41, "SEKCJA: ZAMKNIETA"),
    ("kTextMotor", 42, "SILNIK: OFF"),
    ("kTextMixer", 43, "MIESZADLO: OFF"),
    ("kTextLed", 44, "LED: OFF"),
    ("kTextPwm", 45, "PWM: 0%"),
]

# Soft keys on the right edge, top to bottom: (C++ name, label)
# Key objects get IDs 10.., their labels 20..
SOFT_KEYS = [
    ("kKeyOpen", "OTWORZ"),
    ("kKeyClose", "ZAMKNIJ"),
    ("kKeyMotor", "SILNIK"),
    ("kKeyMixer", "MIESZ."),
    ("kKeyLed", "LED"),
    ("kKeyPwmUp", "PWM +"),
    ("kKeyPwmDown", "PWM -"),
    ("kKeyAutoMotor", "AUTO S."),
    ("kKeyAutoMixer", "AUTO M."),
]

# Joystick (AUX-N) functions: (C++ name, object ID). The VT assigns joystick buttons to them.
AUX_FUNCTIONS = [
    ("kAuxOpen", 2001),
    ("kAuxClose", 2002),
    ("kAuxMotorToggle", 2003),
    ("kAuxMixerToggle", 2004),
    ("kAuxLedToggle", 2005),
]


def u8(value):
    return struct.pack("<B", value)


def u16(value):
    return struct.pack("<H", value)


def header(object_id, object_type):
    return u16(object_id) + u8(object_type)


def child_refs(children):
    """Each child reference is object ID, x and y (6 bytes)."""
    return b"".join(u16(object_id) + struct.pack("<hh", x, y) for object_id, x, y in children)


def working_set():
    return (header(WORKING_SET_ID, WORKING_SET)
            + u8(WHITE) + u8(1) + u16(DATA_MASK_ID)   # background, selectable, active mask
            + u8(1) + u8(0) + u8(1)                    # child, macro and language counts
            + child_refs([(DATA_MASK_ID, 0, 0)])
            + b"pl")


def data_mask(children):
    return (header(DATA_MASK_ID, DATA_MASK)
            + u8(WHITE) + u16(SOFT_KEY_MASK_ID)
            + u8(len(children)) + u8(0)
            + child_refs(children))


def soft_key_mask(key_ids):
    """Soft key masks reference keys by ID only, no positions."""
    return (header(SOFT_KEY_MASK_ID, SOFT_KEY_MASK)
            + u8(SILVER) + u8(len(key_ids)) + u8(0)
            + b"".join(u16(key_id) for key_id in key_ids))


def key(key_id, label_id):
    return (header(key_id, KEY)
            + u8(SILVER) + u8(0)                       # background, key code
            + u8(1) + u8(0)                            # child and macro counts
            + child_refs([(label_id, 2, 9)]))


def output_string(object_id, width, height, background, font_id, text):
    value = text.encode("latin-1")
    return (header(object_id, OUTPUT_STRING)
            + u16(width) + u16(height) + u8(background) + u16(font_id)
            + u8(0)                                    # options: opaque background, no wrap
            + u16(NULL_ID)                             # no variable reference
            + u8(0)                                    # justification: left, top
            + u16(len(value)) + value
            + u8(0))                                   # macro count


def font_attributes(font_id, size):
    return (header(font_id, FONT_ATTRIBUTES)
            + u8(BLACK) + u8(size)
            + u8(0)                                    # ISO 8859-1
            + u8(0) + u8(0))                           # style, macro count


def aux_function(object_id):
    return (header(object_id, AUX_FUNCTION_TYPE2)
            + u8(WHITE) + u8(FUNCTION_BOOLEAN_MOMENTARY)
            + u8(0))                                   # no assigned inputs yet


def build_pool():
    key_ids = [10 + index for index in range(len(SOFT_KEYS))]
    label_ids = [20 + index for index in range(len(SOFT_KEYS))]

    objects = [
        working_set(),
        data_mask([(TITLE_ID, 10, 8)] + [(line_id, 10, 72 + 38 * index)
                                         for index, (_, line_id, _) in enumerate(STATUS_LINES)]),
        soft_key_mask(key_ids),
    ]
    objects += [key(key_id, label_id) for key_id, label_id in zip(key_ids, label_ids)]
    objects += [output_string(label_id, 56, 12, SILVER, FONT_SMALL_ID, label)
                for label_id, (_, label) in zip(label_ids, SOFT_KEYS)]
    objects.append(output_string(TITLE_ID, 400, 32, WHITE, FONT_BIG_ID, "SIEWNIK S1"))
    objects += [output_string(line_id, 400, 24, WHITE, FONT_MEDIUM_ID, text.ljust(STATUS_LINE_LENGTH))
                for _, line_id, text in STATUS_LINES]
    objects += [font_attributes(FONT_BIG_ID, FONT_24X32),
                font_attributes(FONT_MEDIUM_ID, FONT_16X24),
                font_attributes(FONT_SMALL_ID, FONT_8X12)]
    objects += [aux_function(object_id) for _, object_id in AUX_FUNCTIONS]
    return b"".join(objects)


def object_length(pool, pos):
    """Returns the byte length of the object at pos, using the parser's layout rules."""
    object_type = pool[pos + 2]
    if object_type == WORKING_SET:
        children, macros, languages = pool[pos + 7], pool[pos + 8], pool[pos + 9]
        return 10 + 6 * children + 2 * macros + 2 * languages
    if object_type == DATA_MASK:
        children, macros = pool[pos + 6], pool[pos + 7]
        return 8 + 6 * children + 2 * macros
    if object_type == SOFT_KEY_MASK:
        children, macros = pool[pos + 4], pool[pos + 5]
        return 6 + 2 * children + 2 * macros
    if object_type == KEY:
        children, macros = pool[pos + 5], pool[pos + 6]
        return 7 + 6 * children + 2 * macros
    if object_type == OUTPUT_STRING:
        value_length = struct.unpack_from("<H", pool, pos + 14)[0]
        macros = pool[pos + 16 + value_length]
        return 17 + value_length + 2 * macros
    if object_type == FONT_ATTRIBUTES:
        return 8 + 2 * pool[pos + 7]
    if object_type == AUX_FUNCTION_TYPE2:
        return 6 + 6 * pool[pos + 5]
    raise ValueError(f"object type {object_type} at offset {pos} is not handled by the generator")


def check_pool(pool):
    """Walks the pool object by object and checks sizes and references. Returns the object count."""
    object_ids = set()
    references = []
    pos = 0
    count = 0
    while pos < len(pool):
        object_id = struct.unpack_from("<H", pool, pos)[0]
        object_type = pool[pos + 2]
        length = object_length(pool, pos)
        object_ids.add(object_id)
        count += 1

        if object_type in (WORKING_SET, DATA_MASK):
            children = pool[pos + 7] if object_type == WORKING_SET else pool[pos + 6]
            start = pos + (10 if object_type == WORKING_SET else 8)
            references += [struct.unpack_from("<H", pool, start + 6 * index)[0] for index in range(children)]
        elif object_type == KEY:
            references.append(struct.unpack_from("<H", pool, pos + 7)[0])
        elif object_type == SOFT_KEY_MASK:
            references += [struct.unpack_from("<H", pool, pos + 6 + 2 * index)[0] for index in range(pool[pos + 4])]

        pos += length

    if pos != len(pool):
        raise ValueError(f"pool parsed to {pos} bytes, but it has {len(pool)}")
    missing = sorted(set(references) - object_ids)
    if missing:
        raise ValueError(f"references to missing objects: {missing}")
    return count


def id_constants():
    """(C++ name, object ID) pairs written to object_pool_ids.hpp."""
    constants = [
        ("kWorkingSet", WORKING_SET_ID),
        ("kDataMask", DATA_MASK_ID),
        ("kSoftKeyMask", SOFT_KEY_MASK_ID),
        ("kTitle", TITLE_ID),
    ]
    constants += [(name, 10 + index) for index, (name, _) in enumerate(SOFT_KEYS)]
    constants += [(name, line_id) for name, line_id, _ in STATUS_LINES]
    constants += AUX_FUNCTIONS
    return constants


def header_text():
    lines = [
        "// Generated by firmware/tools/gen_object_pool.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace iso::object_pool_ids {",
        "",
    ]
    lines += [f"constexpr std::uint16_t {name} = {object_id};" for name, object_id in id_constants()]
    lines += ["", "}  // namespace iso::object_pool_ids", ""]
    return "\n".join(lines)


def main():
    pool = build_pool()
    object_count = check_pool(pool)
    (ISOBUS_DIR / "object_pool.iop").write_bytes(pool)
    (ISOBUS_DIR / "object_pool_ids.hpp").write_text(header_text(), encoding="utf-8", newline="\n")
    print(f"object pool: {len(pool)} bytes, {object_count} objects")


if __name__ == "__main__":
    main()
