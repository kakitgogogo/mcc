#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "buffer.h"
#include "lexer.h"
#include "parser.h"
#include "preprocessor.h"
#include "generator.h"
using namespace std;

#define MAX_INPUT_FILES 100

Buffer cmd_define_buf;
bool preprocessing_only = false;
bool compile_only = false;
bool do_not_link = false;
char* output_file = nullptr;
vector<char*> include_path;
vector<char*> input_files;
vector<char*> asm_files;
vector<char*> obj_files;

static void usage() {
    fprintf(stderr,
    "Usage: mcc [options] file...\n"
    "Options:\n"
    "-h                       Display this information\n"
    "-E                       Preprocess only; do not compile, assemble or link\n"
    "-S                       Compile only; do not assemble or link\n"
    "-c                       Compile and assemble, but do not link\n"
    "-o <file>                Place the output into <file>\n"
    "-I <path>                add include path\n"
    "-D <name>[=def]          Predefine name as a macro\n"
    "-U <name>                Undefine name\n"
    );
    exit(1);
}

static void arg_parse(int argc, char* argv[]) {
    while(true) {
        int opt = getopt(argc, argv, "hESco:I:D:U:");
        if(opt == -1) break;
        switch(opt) {
        case 'h': usage();
        case 'E': preprocessing_only = true; break;
        case 'S': compile_only = true; break;
        case 'c': do_not_link = true; break;
        case 'o': output_file = optarg; break;
        case 'I': include_path.push_back(optarg); break;
        case 'D': {
            char* p = strchr(optarg, '=');
            if(p) *p = ' ';
            cmd_define_buf.write("#define %s\n", optarg);
            break;
        }
        case 'U': {
            cmd_define_buf.write("#undef %s\n", optarg);
            break;
        }
        default:
            usage();
        }
    }
    if(!preprocessing_only && !compile_only && !do_not_link && !output_file) {
        fprintf(stderr, "One of -E, -S, -c or -o must be specified\n");
        exit(1);
    }
    if(optind >= argc) {
        usage();
    }
    int i = optind;
    while(i < argc) {
        input_files.push_back(argv[i++]);
    }
}

static void check_input_files() {
    for(auto input_file:input_files) {
        char* end = input_file + strlen(input_file) - 1;
        if (*end != 'c') {
            fprintf(stderr, "filename suffix is not .c\n");
            exit(1);
        }
    }
}

static char* replace_suffix(char* filename, char suffix) {
    char* res = strdup(filename);
    char* end = res + strlen(res) - 1;
    *end = suffix;
    return res;
}

static void exit_handler() {
    if(compile_only) return;
    for(auto asm_file:asm_files) {
        unlink(asm_file);
    }
}

int main(int argc, char* argv[]) {
    arg_parse(argc, argv);
    if(atexit(exit_handler))
        perror("atexit");
    if(input_files.size() > MAX_INPUT_FILES) {
        fprintf(stderr, "The number of input files should not exceed %d\n", MAX_INPUT_FILES);
        exit(1);
    }
    for(auto input_file:input_files) {
        Lexer lexer(input_file);
        if(cmd_define_buf.size() > 0) {
            lexer.get_fileset().push_string(cmd_define_buf.data());
        }

        Preprocessor preprocessor(&lexer);
        if(preprocessing_only) {
            cerr << "#" << input_file << endl;
            while(true) {
                TokenPtr tok = preprocessor.get_token();
                if(tok->kind == TEOF) break;
                if (tok->begin_of_line)
                    cerr << "\n";
                if (tok->leading_space)
                    cerr << " ";
                cerr << tok->to_string();
            }
            cerr << endl;
            continue;
        }

        Parser parser(&preprocessor);

        char* asm_file = replace_suffix(input_file, 's');
        asm_files.push_back(asm_file);
        Generator generator(asm_file, &parser);
        generator.run();
        if(compile_only) {
            continue;
        }

        char* obj_file = replace_suffix(input_file, 'o');
        obj_files.push_back(obj_file);
        pid_t pid = fork();
        if (pid < 0) perror("fork");
        if (pid == 0) {
            execlp("as", "as", "-o", obj_file, "-c", asm_file, (char *)NULL);
            perror("execlp failed");
        }
        int status;
        waitpid(pid, &status, 0);
        if (status < 0)
            perror("as failed");
    }

    if(preprocessing_only || compile_only || do_not_link) {
        return 0;
    }

    int i = 3;
    char* gcc_cmd_args[MAX_INPUT_FILES+1];
    gcc_cmd_args[0] = "gcc";
    gcc_cmd_args[1] = "-o";
    gcc_cmd_args[2] = output_file;
    for(auto obj_file:obj_files) {
        gcc_cmd_args[i++] = obj_file;
    }
    gcc_cmd_args[i] = NULL;

    pid_t pid = fork();
    if (pid < 0) perror("fork");
    if (pid == 0) {
        execvp("gcc", gcc_cmd_args);
        perror("execvp failed");
    }
    int status;
    waitpid(pid, &status, 0);
    if (status < 0)
        perror("gcc failed");

    for(auto obj_file:obj_files) {
        unlink(obj_file);
    }

    return 0;
}
