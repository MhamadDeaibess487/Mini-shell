"""Linux/POSIX terminal integration tests; Python standard library only."""
import os
from pathlib import Path
import pty
import re
import select
import signal
import subprocess
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Shell:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(ROOT)
            modes = termios.tcgetattr(0)
            modes[3] &= ~termios.ECHO
            termios.tcsetattr(0, termios.TCSANOW, modes)
            os.execv(str(ROOT / "shell"), ["shell"])
        self.pending = b""
        self.groups = set()
        self.prompt()

    def send(self, command):
        os.write(self.fd, command.encode())

    def prompt(self, timeout=4):
        deadline = time.monotonic() + timeout
        while b"shell> " not in self.pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f"prompt timeout: {self.pending!r}")
            ready, _, _ = select.select([self.fd], [], [], remaining)
            if not ready:
                continue
            try:
                chunk = os.read(self.fd, 65536)
            except OSError as exc:
                raise AssertionError(f"shell exited: {self.pending!r}") from exc
            if not chunk:
                raise AssertionError("shell closed its terminal")
            self.pending += chunk
        output, self.pending = self.pending.split(b"shell> ", 1)
        text = output.decode(errors="replace")
        self.groups.update(int(pid) for pid in re.findall(r"\[\d+\] (\d+)", text))
        return text

    def command(self, command):
        self.send(command + "\n")
        return self.prompt()

    def no_prompt(self, duration=0.15):
        ready, _, _ = select.select([self.fd], [], [], duration)
        if ready:
            self.pending += os.read(self.fd, 65536)
        if b"shell> " in self.pending:
            raise AssertionError(f"unexpected prompt: {self.pending!r}")

    def close(self):
        # Include foreground groups if an assertion failed before a jobs listing.
        children = Path(f"/proc/{self.pid}/task/{self.pid}/children")
        if children.exists():
            for pid in children.read_text().split():
                try:
                    self.groups.add(os.getpgid(int(pid)))
                except ProcessLookupError:
                    pass
        for group in self.groups:
            if group > 0 and group != os.getpgrp():
                try:
                    os.killpg(group, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        try:
            self.send("quit\n")
        except OSError:
            pass
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            pid, _ = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                break
            time.sleep(0.01)
        else:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        os.close(self.fd)


class JobControlTests(unittest.TestCase):
    def setUp(self):
        self.shell = Shell()

    def tearDown(self):
        self.shell.close()

    def test_invalid_ids_and_missing_arguments(self):
        for command in ("fg", "bg", "fg 0", "bg -1", "fg 16", "fg abc",
                        "bg %", "fg 1x", "bg 999999999999999999999999999999",
                        "fg 1 2", "fg %1", "bg 1", "kill"):
            with self.subTest(command=command):
                self.assertRegex(self.shell.command(command), r"usage|invalide|inexistant")
        self.assertIn("alive", self.shell.command("echo alive"))

    def test_foreground_wait_and_background_return(self):
        output = self.shell.command("sleep 2 &")
        self.assertRegex(output, r"\[1\] \d+")
        self.assertIn("Background", self.shell.command("jobs"))
        self.shell.send("sleep 0.4\n")
        self.shell.no_prompt()
        self.shell.prompt()
        self.assertIn("sleep", self.shell.command("jobs"))

    def test_fg_running_job_waits_for_selected_group(self):
        self.shell.command("sleep 0.7 &")
        self.shell.command("sleep 0.1 &")
        self.shell.send("fg %1\n")
        self.shell.no_prompt(0.25)
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_ctrl_z_bg_fg_ctrl_c(self):
        self.shell.send("sleep 10\n")
        self.shell.no_prompt(0.1)
        self.shell.send("\x1a")
        self.shell.prompt()
        self.assertIn("Stopped", self.shell.command("jobs"))
        self.shell.command("bg %1")
        self.assertIn("Background", self.shell.command("jobs"))
        self.shell.send("fg 1\n")
        self.shell.no_prompt()
        self.shell.send("\x03")
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))
        self.assertIn("alive", self.shell.command("echo alive"))

    def test_fg_resumes_stopped_job_directly(self):
        self.shell.send("sleep 10\n")
        self.shell.no_prompt(0.1)
        self.shell.send("\x1a")
        self.shell.prompt()
        self.shell.send("fg %1\n")
        self.shell.no_prompt()
        self.shell.send("\x03")
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_pipeline_is_one_job(self):
        self.shell.send("sleep 10 | cat\n")
        self.shell.no_prompt(0.1)
        self.shell.send("\x1a")
        self.shell.prompt()
        jobs = self.shell.command("jobs")
        self.assertEqual(jobs.count("PID="), 1)
        self.assertIn("Stopped", jobs)
        self.shell.command("bg 1")
        self.shell.send("fg 1\n")
        self.shell.no_prompt()
        self.shell.send("\x03")
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_terminal_input_and_background_reader(self):
        self.shell.command("cat &")
        time.sleep(0.1)
        self.assertIn("Stopped", self.shell.command("jobs"))
        self.shell.send("fg %1\n")
        self.shell.no_prompt(0.1)
        self.shell.send("terminal-input\n")
        self.shell.no_prompt(0.1)
        self.assertIn(b"terminal-input", self.shell.pending)
        self.shell.send("\x04")
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_fast_children_do_not_leave_jobs(self):
        for index in range(60):
            self.assertNotIn("pleine", self.shell.command("true &"))
            if index % 10 == 9:
                time.sleep(0.1)
        self.shell.command("sleep 0.1")
        self.assertNotIn("PID=", self.shell.command("jobs"))
        self.assertIn("ok", self.shell.command("echo ok | cat | cat"))

    def test_pipeline_waits_for_every_member(self):
        self.shell.send("true | sleep 0.4\n")
        self.shell.no_prompt()
        self.shell.prompt()
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_background_children_are_reaped_while_idle(self):
        self.shell.command("sleep 0.1&")
        time.sleep(0.25)
        children = Path(f"/proc/{self.shell.pid}/task/{self.shell.pid}/children")
        if children.exists():
            self.assertEqual(children.read_text().strip(), "")
        self.assertNotIn("PID=", self.shell.command("jobs"))

    def test_terminal_modes_survive_stop_and_resume(self):
        self.shell.send("python3 -u tests/helpers/terminal_job.py\n")
        self.shell.prompt()
        self.assertIn("Stopped", self.shell.command("jobs"))
        self.assertIn("MODES_OK", self.shell.command("fg 1"))
        self.assertFalse(termios.tcgetattr(self.shell.fd)[3] & termios.ECHO)

    def test_table_full_then_reusable(self):
        for index in range(1, 16):
            self.assertIn(f"[{index}]", self.shell.command("sleep 30 &"))
        self.assertIn("table des jobs pleine", self.shell.command("sleep 30 &"))
        self.shell.command("kill %1")
        time.sleep(0.1)
        self.assertIn("[1]", self.shell.command("sleep 30 &"))

    def test_reject_builtin_pipeline_without_launching_children(self):
        for command in ("fg 1 | cat", "echo bad | bg 1", "jobs &", "jobs > unwanted"):
            self.assertIn("commandes internes", self.shell.command(command))
        self.assertIn("ok", self.shell.command("echo ok"))

    def test_parser_errors_do_not_crash(self):
        for command in ("|", "echo x |", "echo >", "sleep 1 & extra",
                        "echo > &", "cat < |", "&", "echo &&"):
            self.assertIn("error:", self.shell.command(command))

    def test_completed_job_is_rejected(self):
        self.shell.command("sleep 0.1 &")
        time.sleep(0.2)
        self.assertIn("inexistant", self.shell.command("fg 1"))


class BatchTests(unittest.TestCase):
    def test_eof_and_redirections(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "text"
            commands = f"echo preserved > {path}\ncat < {path} | cat\necho eof"
            result = subprocess.run([str(ROOT / "shell")], input=commands,
                                    text=True, capture_output=True, timeout=4)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("preserved", result.stdout)
            self.assertIn("eof", result.stdout)
            self.assertEqual(path.read_text(), "preserved\n")


if __name__ == "__main__":
    unittest.main(verbosity=2)
