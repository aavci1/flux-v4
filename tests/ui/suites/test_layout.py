import unittest

from flux_test_client import find_node, start_test_app_skip


class TestLayout(unittest.TestCase):
    def test_title_and_bounds(self) -> None:
        app = start_test_app_skip("test_layout")
        try:
            tree = app.client.get_ui()
            title = find_node(tree, focus_key="title")
            self.assertIsNotNone(title)
            assert title is not None
            self.assertIn("Layout", title.get("text", ""))
            b = title["bounds"]
            self.assertGreater(b["w"], 10.0)
            self.assertGreater(b["h"], 10.0)
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
