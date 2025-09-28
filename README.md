# TUS-Manager — The Ultimate Shell Manager

> A compact and practical shell session manager for penetration testers.

## Overview

TUS-Manager (TUSM) is a C-based shell management utility that helps penetration testers and red-teamers manage bind and reverse shell sessions created with **netcat (nc)**. It supports multiple concurrent sessions, interactive switching, file transfers, and shell upgrades.


## Features

- Start listeners to catch reverse shells (`nc -lnp` under the hood)
- Connect to remote bind shells (`nc host port`)
- Manage multiple concurrent sessions
- Interactive session handling (background, switch, terminate)
- Upload files via Base64
- Upgrade remote shells to interactive PTYs (python, script, bash)
- Simple CLI with helpful command menu


## Requirements

- GCC (for compilation)
- `nc` (netcat) — traditional or OpenBSD netcat
- `rlwrap` — optional but automatically used for readline support if installed
- POSIX shell environment (Linux/Unix)


## Installation

### 1. Update packages and install Netcat
Netcat is required for connectivity.
```bash
sudo apt update && sudo apt install -y netcat-openbsd
```

### 2. Install rlwrap
rlwrap provides readline-style editing and command history.
```bash
sudo apt install rlwrap
```

### 3. Clone the repository
Make sure Git is installed before cloning.
```bash
git clone https://github.com/TUKS-Project/TUS-Manager.git
cd TUS-Manager
```

### 4. Compile the program
```bash
make
```



## Commands reference

- `listen <port>` — Start a reverse shell listener.
- `connect <ip> <port>` — Connect to a bind shell.
- `list` — Show all sessions.
- `use <id>` — Interact with a session.
- `kill <id>` — Terminate a session.
- `upload <id> <local> <remote>` — Upload a file to remote target.
- `upgrade <id>` — Upgrade shell to interactive PTY.
- `help` — Show command menu.
- `clear` — Clear screen.
- `/BG` — Send active session to background.
- `exit` — Quit TUSM.


## Usage

1. Start the tool
```bash
Tusm
```
2. User Interface
![Logo](images/UX.png)





## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
