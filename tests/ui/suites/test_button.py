import unittest

from flux_test_client import center_of_bounds, find_node, start_test_app_skip


class TestButton(unittest.TestCase):
    def test_click_increments_counter(self) -> None:
        app = start_test_app_skip("test_button")
        try:
            c = app.client
            tree = c.get_ui()
            btn = find_node(tree, focus_key="tap-me")
            self.assertIsNotNone(btn)
            assert btn is not None
            x, y = center_of_bounds(btn)
            c.click(x, y)
            tree2 = c.get_ui()
            label = find_node(tree2, focus_key="count-label")
            self.assertIsNotNone(label)
            assert label is not None
            self.assertIn("count:1", label.get("text", ""))
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
