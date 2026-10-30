"""Stop with changed terminal modes; fg must restore them before SIGCONT."""
import os
import signal
import termios

modes = termios.tcgetattr(0)
modes[3] |= termios.ECHO
termios.tcsetattr(0, termios.TCSANOW, modes)
os.kill(os.getpid(), signal.SIGTSTP)
if termios.tcgetattr(0)[3] & termios.ECHO:
    print("MODES_OK")
else:
    raise SystemExit("terminal modes lost")
