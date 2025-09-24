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
git clone https://github.com/yourusername/tuks.git
cd tuks
```


# Compile
```bash
gcc tuks.c -o TUKS
```

--- 

## Usage

# Start the tool
```bash
/TUKS
```


Inside the TUKS shell:

use 1         # Switch to session 1
background    # Put current session in background
kill 2        # Kill session 2
list          # Show all sessions

---

## Development If you’d like to contribute, see [CONTRIBUTING.md](CONTRIBUTING.md). 

--- 

## License This project is licensed under the [MIT License](LICENSE).
