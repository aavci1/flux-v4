import time
import unittest

from flux_test_client import center_of_bounds, find_node, start_test_app_skip


class TestOverlay(unittest.TestCase):
    def test_modal_open_close(self) -> None:
        app = start_test_app_skip("test_overlay")
        try:
            c = app.client
            tree = c.get_ui()
            open_btn = find_node(tree, focus_key="open-dialog")
            self.assertIsNotNone(open_btn)
            assert open_btn is not None
            c.click(*center_of_bounds(open_btn))
            modal = None
            for _ in range(60):
                time.sleep(0.05)
                tree2 = c.get_ui()
                modal = find_node(tree2, focus_key="modal-title")
                if modal is not None:
                    break
            self.assertIsNotNone(modal)
            assert modal is not None
            self.assertIn("Modal", modal.get("text", ""))
            close_btn = find_node(tree2, focus_key="close-dialog")
            self.assertIsNotNone(close_btn)
            assert close_btn is not None
            c.click(*center_of_bounds(close_btn))
            tree3 = c.get_ui()
            self.assertIsNone(find_node(tree3, focus_key="modal-title"))
        finally:
            app.close()


if __name__ == "__main__":
    unittest.main()
