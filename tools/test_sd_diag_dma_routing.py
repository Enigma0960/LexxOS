"""Static routing regression for the RK3506 sd-diag command tree.

The command executes in a target-only runtime. This check verifies that the
read-only diagnostics are reachable in the DMA branch and never in FIFO.
"""

from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "src/lexxos/apps/storage/rk3506/picosd_program.cpp"


class SdDiagRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = SOURCE.read_text(encoding="utf-8")

    def test_dma_diagnostics_are_reachable(self):
        dma = self.text.split('if (action == "dma") {', 1)[1].split('if (action == "profile") {', 1)[0]
        self.assertEqual(dma.count('if (option == "irq-info") {'), 1)
        self.assertLess(dma.index('if (option == "irq-info") {'), dma.index('if (option != "status") {'))
        self.assertEqual(dma.count('if (option == "wait") {'), 1)

    def test_fifo_has_only_its_own_subcommands(self):
        fifo = self.text.split('if (action == "fifo") {', 1)[1].split('if (action == "mode") {', 1)[0]
        self.assertNotIn('if (option == "irq-info") {', fifo)
        self.assertNotIn('if (option == "wait") {', fifo)
        self.assertIn('if (option == "reset") {', fifo)
        self.assertIn('option == "safe" || option == "fast"', fifo)


if __name__ == "__main__":
    unittest.main()
