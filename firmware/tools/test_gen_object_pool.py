import struct
import unittest

import gen_object_pool as pool_generator


def objects_by_id(pool):
    objects = {}
    offset = 0
    while offset < len(pool):
        object_id, object_type = struct.unpack_from("<HB", pool, offset)
        length = pool_generator.object_length(pool, offset)
        objects[object_id] = (object_type, pool[offset:offset + length])
        offset += length
    return objects


class ObjectPoolTests(unittest.TestCase):
    def test_working_set_references_data_mask_as_active_mask_not_child(self):
        objects = objects_by_id(pool_generator.build_pool())
        _, working_set = objects[pool_generator.WORKING_SET_ID]
        self.assertEqual(struct.unpack_from("<H", working_set, 5)[0], pool_generator.DATA_MASK_ID)
        self.assertEqual(working_set[7], 1)
        label_id = struct.unpack_from("<H", working_set, 10)[0]
        label_type, label = objects[label_id]
        self.assertEqual(label_type, pool_generator.OUTPUT_STRING)
        self.assertEqual(struct.unpack_from("<HH", label, 3), (56, 12))
        self.assertEqual(label[16:23], b"SIEWNIK")
        self.assertEqual(objects[pool_generator.DATA_MASK_ID][0], pool_generator.DATA_MASK)

    def test_check_pool_rejects_missing_active_mask(self):
        pool = bytearray(pool_generator.build_pool())
        struct.pack_into("<H", pool, 5, 999)
        with self.assertRaisesRegex(ValueError, "references to missing objects"):
            pool_generator.check_pool(pool)

    def test_aux_functions_have_distinct_visible_labels(self):
        objects = objects_by_id(pool_generator.build_pool())
        labels = set()
        for index, (_, object_id) in enumerate(pool_generator.AUX_FUNCTIONS):
            object_type, function = objects[object_id]
            self.assertEqual(object_type, pool_generator.AUX_FUNCTION_TYPE2)
            self.assertEqual(function[5], 1)
            label_id = struct.unpack_from("<H", function, 6)[0]
            labels.add(label_id)
            label_type, label = objects[label_id]
            self.assertEqual(label_type, pool_generator.OUTPUT_STRING)
            text_length = struct.unpack_from("<H", label, 14)[0]
            self.assertEqual(label[16:16 + text_length].decode("latin-1"),
                             pool_generator.SOFT_KEYS[index][1])
        self.assertEqual(len(labels), len(pool_generator.AUX_FUNCTIONS))

    def test_soft_keys_do_not_use_reserved_ack_code(self):
        objects = objects_by_id(pool_generator.build_pool())
        key_codes = []
        for index in range(len(pool_generator.SOFT_KEYS)):
            object_type, key = objects[10 + index]
            self.assertEqual(object_type, pool_generator.KEY)
            self.assertNotEqual(key[4], 0)
            key_codes.append(key[4])
        self.assertEqual(len(set(key_codes)), len(key_codes))

    def test_check_pool_rejects_missing_aux_child(self):
        pool = bytearray(pool_generator.build_pool())
        offset = 0
        while pool[offset + 2] != pool_generator.AUX_FUNCTION_TYPE2:
            offset += pool_generator.object_length(pool, offset)
        struct.pack_into("<H", pool, offset + 6, pool_generator.NULL_ID)
        with self.assertRaisesRegex(ValueError, "references to missing objects"):
            pool_generator.check_pool(pool)

    def test_generated_pool_matches_committed_binary(self):
        self.assertEqual((pool_generator.ISOBUS_DIR / "object_pool.iop").read_bytes(),
                         pool_generator.build_pool())


if __name__ == "__main__":
    unittest.main()
