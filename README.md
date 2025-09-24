# TUS-Manager
The Ultimate Shell Manager
# TUSM - The Ultimate Shell Manager 

## Overview

TUSM is a simple shell manager tool for educational and ethical penetration testing purposes. It manages **bind shells** and **reverse shells** using netcat (nc) under the hood. With TUSM, you can: 

* Listen for reverse shells
* Connect to bind shells
* Manage multiple sessions
* List, kill, use, or background active sessions


---

## Features

* Start a listener for reverse shells
* Connect to a bind shell on a remote host
* Manage multiple concurrent shell sessions
* Simple command interface
* Session management (switch, background, terminate)


---

## Installation

'''bash
# Clone the repository
git clone https://github.com/yourusername/tuks.git
cd tuks
'''


# Compile
gcc tuks.c -o tuks


--- 

## Usage

bash
# Start the tool
./tuks

Inside the TUKS shell:

use 1         # Switch to session 1
background    # Put current session in background
kill 2        # Kill session 2
list          # Show all sessions

---

## Development If you’d like to contribute, see [CONTRIBUTING.md](CONTRIBUTING.md). 

--- 

## License This project is licensed under the [MIT License](LICENSE). 
