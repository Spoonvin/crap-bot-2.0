"""UCI subprocess regressions; uses only the Python standard library."""

import queue
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path


ENGINE = str(Path(sys.argv.pop(1) if len(sys.argv) > 1 else "./app").resolve())
POSITION = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"


class UciTest(unittest.TestCase):
    def setUp(self):
        self.engine = subprocess.Popen(
            [ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True
        )
        self.lines = queue.Queue()
        self.reader = threading.Thread(target=self.read_output, daemon=True)
        self.reader.start()
        self.send("uci")
        self.handshake = self.until("uciok")
        self.send("setoption name Hash value 1", "isready")
        self.until("readyok")

    def read_output(self):
        for line in self.engine.stdout:
            self.lines.put(line.strip())

    def send(self, *commands):
        self.engine.stdin.write("\n".join(commands) + "\n")
        self.engine.stdin.flush()

    def until(self, prefix, timeout=5):
        deadline = time.monotonic() + timeout
        lines = []
        while True:
            line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            lines.append(line)
            if line.startswith(prefix):
                return lines

    def tearDown(self):
        try:
            self.send("quit")
            self.engine.wait(timeout=5)
        finally:
            if self.engine.poll() is None:
                self.engine.kill()
                self.engine.wait()
            self.reader.join(timeout=1)
            self.engine.stdin.close()
            self.engine.stdout.close()
        self.assertEqual(self.engine.returncode, 0)

    def test_options_and_position_history(self):
        for name in ("Move Overhead", "Threads", "Hash"):
            self.assertTrue(any(line.startswith(f"option name {name} type spin ")
                                for line in self.handshake))
        self.send("setoption name Move Overhead value 100",
                  "setoption name Threads value 8",
                  "ucinewgame",
                  "position startpos moves f2f3 e7e5 g2g4",
                  "go movetime 500")
        self.assertEqual(self.until("bestmove")[-1], "bestmove d8h4")

    def test_overhead_and_increment_cannot_exceed_clock(self):
        self.send("setoption name Move Overhead value 5000",
                  f"position fen {POSITION}", "isready")
        self.until("readyok")
        for command in ("go movetime 3000",
                        "go wtime 100 btime 60000 winc 100000 movestogo 1"):
            start = time.monotonic()
            self.send(command)
            self.assertRegex(self.until("bestmove", timeout=1)[-1],
                             r"^bestmove [a-h][1-8][a-h][1-8][qrbn]?$")
            self.assertLess(time.monotonic() - start, 1)

    def test_ready_and_stop_during_search(self):
        self.send(f"position fen {POSITION}", "go movetime 10000", "isready")
        self.assertEqual(self.until("readyok", timeout=1), ["readyok"])
        self.send("stop")
        self.assertRegex(self.until("bestmove", timeout=1)[-1], r"^bestmove [a-h]")
        # Repeated stop must not emit another move, and cancellation must reset.
        self.send("stop", "position startpos", "go movetime 1")
        self.until("bestmove")
        self.send("isready")
        self.assertEqual(self.until("readyok"), ["readyok"])

    def test_infinite_waits_for_stop_even_in_book(self):
        self.send("position startpos", "go infinite", "isready")
        self.assertEqual(self.until("readyok", timeout=1), ["readyok"])
        with self.assertRaises(queue.Empty):
            self.lines.get(timeout=0.1)
        self.send("stop")
        self.until("bestmove", timeout=1)

    def test_terminal_and_promotion_positions(self):
        self.send("position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1", "go movetime 1")
        self.assertEqual(self.until("bestmove")[-1], "bestmove 0000")
        self.send("position fen 8/6P1/8/8/8/5K2/7k/8 w - - 0 1 moves g7g8q",
                  "go movetime 1")
        self.assertRegex(self.until("bestmove")[-1], r"^bestmove h2[g-h][1-3]$")

    def test_quit_during_search(self):
        self.send(f"position fen {POSITION}", "go movetime 10000", "isready")
        self.until("readyok", timeout=1)
        self.send("quit")
        self.engine.wait(timeout=1)
        # tearDown handles an already exited process without writing to its pipe.
        self.send = lambda *commands: None


if __name__ == "__main__":
    unittest.main()
