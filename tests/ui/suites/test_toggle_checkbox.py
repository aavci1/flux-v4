import unittest

from flux_test_client import center_of_bounds, find_node, start_test_app_skip


class TestToggleCheckbox(unittest.TestCase):
    def test_toggle_and_checkbox(self) -> None:
        app = start_test_app_skip("test_toggle_checkbox")
        try:
            c = app.client
            tree = c.get_ui()
            t = find_node(tree, focus_key="wifi-toggle")
            self.assertIsNotNone(t)
            assert t is not None
            c.click(*center_of_bounds(t))
            tree2 = c.get_ui()
            st = find_node(tree2, focus_key="state-label")
            self.assertIsNotNone(st)
            assert st is not None
            self.assertIn("10", st.get("text", ""))

            cb = find_node(tree2, focus_key="agree-box")
            self.assertIsNotNone(cb)
            assert cb is not None
            b = cb["bounds"]
            self.assertGreater(b["w"], 2.0)
            self.assertGreater(b["h"], 2.0)
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
