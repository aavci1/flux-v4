import time
import unittest

from flux_test_client import center_of_bounds, find_node, start_test_app_skip


class TestFocus(unittest.TestCase):
    def test_two_fields(self) -> None:
        app = start_test_app_skip("test_focus")
        try:
            c = app.client
            tree = c.get_ui()
            a = find_node(tree, focus_key="field-a")
            b = find_node(tree, focus_key="field-b")
            self.assertIsNotNone(a)
            self.assertIsNotNone(b)
            assert a is not None and b is not None
            c.click(*center_of_bounds(a))
            time.sleep(0.15)
            c.type_text("x")
            time.sleep(0.15)
            c.click(*center_of_bounds(b))
            time.sleep(0.15)
            c.type_text("y")
            time.sleep(0.2)
            tree2 = c.get_ui()
            out = find_node(tree2, focus_key="combined-label")
            self.assertIsNotNone(out)
            assert out is not None
            self.assertIn("out:x|y", out.get("text", ""))
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
