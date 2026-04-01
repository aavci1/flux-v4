import unittest

from flux_test_client import find_node, start_test_app_skip


class TestSlider(unittest.TestCase):
    def test_slider_present(self) -> None:
        app = start_test_app_skip("test_slider")
        try:
            c = app.client
            tree = c.get_ui()
            sl = find_node(tree, focus_key="level-slider")
            self.assertIsNotNone(sl)
            assert sl is not None
            lab = find_node(tree, focus_key="value-label")
            self.assertIsNotNone(lab)
            assert lab is not None
            self.assertIn("value:", lab.get("text", ""))
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
