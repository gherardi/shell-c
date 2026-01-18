#include "common.h"
#include "parser.h"
#include "builtins.h"
#include "executor.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <sys/stat.h>

// structure to track completion state
typedef struct {
    int phase;              // 0 = builtins, 1 = executables
    int builtin_index;      // current builtin index
    char **path_dirs;       // array of PATH directories
    int path_dir_count;     // number of PATH directories
    int current_dir_index;  // current directory being scanned
    DIR *current_dir;       // current directory handle
    int text_len;           // length of text to match
    char **seen_executables; // list of executables already returned (to avoid duplicates)
    int seen_count;         // number of seen executables
} completion_state_t;

// free completion state resources
static void free_completion_state(completion_state_t *state) {
    if (state->path_dirs != NULL) {
        for (int i = 0; i < state->path_dir_count; i++) {
            free(state->path_dirs[i]);
        }
        free(state->path_dirs);
        state->path_dirs = NULL;
    }
    if (state->current_dir != NULL) {
        closedir(state->current_dir);
        state->current_dir = NULL;
    }
    if (state->seen_executables != NULL) {
        for (int i = 0; i < state->seen_count; i++) {
            free(state->seen_executables[i]);
        }
        free(state->seen_executables);
        state->seen_executables = NULL;
    }
}

// check if executable name was already seen
static bool is_seen(completion_state_t *state, const char *name) {
    for (int i = 0; i < state->seen_count; i++) {
        if (strcmp(state->seen_executables[i], name) == 0) {
            return true;
        }
    }
    return false;
}

// add executable to seen list
static void add_seen(completion_state_t *state, const char *name) {
    state->seen_executables = realloc(state->seen_executables, 
                                      (state->seen_count + 1) * sizeof(char *));
    if (state->seen_executables != NULL) {
        state->seen_executables[state->seen_count] = strdup(name);
        state->seen_count++;
    }
}

// check if file is executable
static bool is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (st.st_mode & S_IXUSR) != 0;
    }
    return false;
}

// generator function for command completion (builtins + executables)
char *command_generator(const char *text, int state) {
    static completion_state_t comp_state = {0};
    
    // initialize on first call
    if (!state) {
        // free any previously allocated resources
        free_completion_state(&comp_state);
        
        comp_state.phase = 0;
        comp_state.builtin_index = 0;
        comp_state.path_dirs = NULL;
        comp_state.path_dir_count = 0;
        comp_state.current_dir_index = 0;
        comp_state.current_dir = NULL;
        comp_state.text_len = strlen(text);
        comp_state.seen_executables = NULL;
        comp_state.seen_count = 0;
        
        // parse PATH environment variable
        char *path_env = getenv("PATH");
        if (path_env != NULL) {
            char path_copy[2048];
            strncpy(path_copy, path_env, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';
            
            // count directories
            int count = 0;
            char *temp = path_copy;
            while (*temp) {
                if (*temp == ':') count++;
                temp++;
            }
            count++; // last directory
            
            comp_state.path_dirs = malloc(count * sizeof(char *));
            if (comp_state.path_dirs != NULL) {
                char *dir = strtok(path_copy, PATH_SEPARATOR);
                while (dir != NULL && comp_state.path_dir_count < count) {
                    comp_state.path_dirs[comp_state.path_dir_count] = strdup(dir);
                    comp_state.path_dir_count++;
                    dir = strtok(NULL, PATH_SEPARATOR);
                }
            }
        }
    }
    
    // phase 0: return builtin matches
    if (comp_state.phase == 0) {
        static const char *builtins[] = {"echo", "exit"};
        static const int builtin_count = 2;
        
        while (comp_state.builtin_index < builtin_count) {
            const char *name = builtins[comp_state.builtin_index++];
            if (strncmp(name, text, comp_state.text_len) == 0) {
                return strdup(name);
            }
        }
        
        // switch to executables phase
        comp_state.phase = 1;
    }
    
    // phase 1: return executable matches from PATH
    if (comp_state.phase == 1) {
        while (comp_state.current_dir_index < comp_state.path_dir_count) {
            // open directory if not already open
            if (comp_state.current_dir == NULL) {
                const char *dir_path = comp_state.path_dirs[comp_state.current_dir_index];
                
                // check if directory exists and is accessible
                if (access(dir_path, R_OK | X_OK) == 0) {
                    comp_state.current_dir = opendir(dir_path);
                }
                
                // if directory doesn't exist or can't be opened, skip it
                if (comp_state.current_dir == NULL) {
                    comp_state.current_dir_index++;
                    continue;
                }
            }
            
            // read directory entries
            struct dirent *entry = readdir(comp_state.current_dir);
            if (entry == NULL) {
                // end of directory, move to next
                closedir(comp_state.current_dir);
                comp_state.current_dir = NULL;
                comp_state.current_dir_index++;
                continue;
            }
            
            // skip hidden files and directories
            if (entry->d_name[0] == '.') {
                continue;
            }
            
            // check if name matches text prefix
            if (strncmp(entry->d_name, text, comp_state.text_len) == 0) {
                // check if already seen (to avoid duplicates across PATH directories)
                if (is_seen(&comp_state, entry->d_name)) {
                    continue;
                }
                
                // build full path and check if executable
                char fullpath[512];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                        comp_state.path_dirs[comp_state.current_dir_index], 
                        entry->d_name);
                
                if (is_executable(fullpath)) {
                    add_seen(&comp_state, entry->d_name);
                    return strdup(entry->d_name);
                }
            }
        }
        
        // cleanup when done
        free_completion_state(&comp_state);
        return NULL;
    }
    
    return NULL;
}

// completion generator function
char **command_completion(const char *text, int start, int end) {
    char **matches = NULL;
    
    // suppress unused parameter warning
    (void)end;
    
    // disable default filename completion
    rl_attempted_completion_over = 1;
    
    // only complete the first word (command name)
    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    }
    
    return matches;
}

int main(int argc, char *argv[]) {
    // disable output buffering for stdout
    setbuf(stdout, NULL);
    
    // set up tab completion
    rl_attempted_completion_function = command_completion;
    // append space after completion
    rl_completion_append_character = ' ';

    while (1) {
        char *user_input = readline("$ ");
        
        if (user_input == NULL) break; // EOF (Ctrl+D)
        
        // skip empty input
        if (strlen(user_input) == 0) {
            free(user_input);
            continue;
        }

        Args args = parse_arguments(user_input);
        
        if (args.count == 0) {
            free(user_input);
            continue;
        }
        
        cmd_handler_t handler = find_builtin_handler(args.args[0]);
        
        if (handler != NULL) {
            execute_with_redirection(handler, (char **)args.args, &args.output_redirect);
        } else {
            handle_external_command((char **)args.args, &args.output_redirect);
        }
        
        free_arguments(&args);
        free(user_input);
    }

    return 0;
}
