#pragma once 

#include <map>
#include <vector>
#include <memory>
#include "ast.h"
#include "utils.h"

class Node;

struct CaseTuple {
    int begin;
    int end;
    char* label;
};

class Scope {
public:
    Scope() {}
    Scope(Scope* scope): 
        global_env(scope->global_env), 
        local_envs(scope->local_envs) {}

    std::shared_ptr<Node> get(char* name);
    std::shared_ptr<Node> get_local(char* name);
    char* get_continue_label();
    char* get_break_label();
    char* get_default_label();
    std::vector<CaseTuple>& get_cases();
    FuncType* get_current_func_type();
    std::map<char*, std::shared_ptr<Node>, cstr_cmp> get_local_env();

    void add(char* name, std::shared_ptr<Node> val);
    void add_global(char* name, std::shared_ptr<Node> val);
    void add_switch_case(CaseTuple c);
    void set_switch_default(char* label);

    void in();
    void in(FuncType* func);
    void out();
    void in_loop(char* lcontinue, char* lbreak);
    void out_loop();
    void in_switch(char* lbreak);
    void out_switch();

    bool islocal() { return local_envs.size() > 0; }
    bool is_in_loop() { return breaks.size() > 0; }
    bool is_in_switch() { return defaults.size() > 0; }

    void clear_local();
    void recover_local();

    Scope* copy();

private:
    std::map<char*, std::shared_ptr<Node>, cstr_cmp> global_env;
    std::vector<std::map<char*, std::shared_ptr<Node>, cstr_cmp>> local_envs;
    std::vector<std::map<char*, std::shared_ptr<Node>, cstr_cmp>> local_envs_backup;

    FuncType* current_func_type;

    // used in loop-scope
    std::vector<char*> continues;
    std::vector<char*> breaks;

    // used in switch-scope
    std::vector<char*> defaults;
    std::vector<std::vector<CaseTuple>> cases;
};