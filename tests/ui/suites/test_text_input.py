import unittest

from flux_test_client import center_of_bounds, find_node, start_test_app_skip


class TestTextInput(unittest.TestCase):
    def test_type_updates_echo(self) -> None:
        app = start_test_app_skip("test_text_input")
        try:
            c = app.client
            tree = c.get_ui()
            field = find_node(tree, focus_key="name-field")
            self.assertIsNotNone(field)
            assert field is not None
            c.click(*center_of_bounds(field))
            c.type_text("flux")
            tree2 = c.get_ui()
            echo = find_node(tree2, focus_key="echo-label")
            self.assertIsNotNone(echo)
            assert echo is not None
            self.assertIn("flux", echo.get("text", ""))
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
