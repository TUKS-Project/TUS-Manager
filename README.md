# TUS-Manager
The Ultimate Shell Manager
# TUSM - The Ultimate Shell Manager 

## Overview

TUSM is a simple shell manager tool for pentestors!
It manages **bind shells** and **reverse shells** using netcat (nc) under the hood. With readline wrapper (rlwrap), you can: 

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


# Clone the repository
```bash
git clone https://github.com/TUKS-Project/TUS-Manager.git
cd TUS-Manager
```


# Compile
```bash
make
```

--- 

## Usage

# Start the tool
```bash
Tusm
```
![Logo](images/UX.png)

Inside the TUKS shell:

use 1         # Switch to session 1
background    # Put current session in background
kill 2        # Kill session 2
list          # Show all sessions



## License This project is licensed under the [MIT License](LICENSE). 

