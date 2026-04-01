import unittest

from flux_test_client import find_node, start_test_app_skip


class TestScroll(unittest.TestCase):
    def test_deep_row_exists_in_tree(self) -> None:
        app = start_test_app_skip("test_scroll")
        try:
            c = app.client
            tree = c.get_ui()
            row = find_node(tree, text="Row 40 — scroll harness")
            self.assertIsNotNone(row)
            title = find_node(tree, focus_key="scroll-title")
            self.assertIsNotNone(title)
            # Scroll wheel at mid viewport.
            c.scroll(220, 200, 0, -800)
            tree2 = c.get_ui()
            row2 = find_node(tree2, text="Row 40 — scroll harness")
            self.assertIsNotNone(row2)
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
