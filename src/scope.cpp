#include <string.h>
#include <assert.h>
#include "scope.h"

std::shared_ptr<Node> Scope::get(char* name) {
    for(int i = local_envs.size()-1; i >= 0; --i) {
        if(local_envs[i].find(name) != local_envs[i].end()) {
            return local_envs[i][name];
        }
    }
    if(global_env.find(name) != global_env.end()) {
        return global_env[name];
    }
    return nullptr;
}

std::shared_ptr<Node> Scope::get_local(char* name) {
    if(local_envs.back().find(name) != local_envs.back().end()) {
        return local_envs.back()[name];
    }
    return nullptr;
}


char* Scope::get_continue_label() {
    assert(continues.size() > 0);
    return continues.back();
}

char* Scope::get_break_label() {
    assert(breaks.size() > 0);
    return breaks.back();
}

char* Scope::get_default_label() {
    assert(defaults.size() > 0);
    return defaults.back();
}

std::vector<CaseTuple>& Scope::get_cases() {
    assert(cases.size() > 0);
    return cases.back();
}

FuncType* Scope::get_current_func_type() {
    return current_func_type;
}

std::map<char*, std::shared_ptr<Node>, cstr_cmp> Scope::get_local_env() {
    return local_envs.back();
}


void Scope::add(char* name, std::shared_ptr<Node> val) {
    if(local_envs.size() > 0)
        local_envs.back()[name] = val;
    else 
        global_env[name] = val;
}

void Scope::add_global(char* name, std::shared_ptr<Node> val) {
    global_env[name] = val;
} 

void Scope::add_switch_case(CaseTuple c) {
    cases.back().push_back(c);
}

void Scope::set_switch_default(char* label) {
    defaults.back() = label;
}

void Scope::in() {
    local_envs.push_back(std::map<char*, std::shared_ptr<Node>, cstr_cmp>());
}

void Scope::in(FuncType* func) {
    current_func_type = func;
    in();
}

void Scope::out() {
    if(local_envs.size() > 0)
        local_envs.pop_back();
}

void Scope::in_loop(char* lcontinue, char* lbreak) {
    continues.push_back(lcontinue);
    breaks.push_back(lbreak);
    in();
}

void Scope::out_loop() {
    continues.pop_back();
    breaks.pop_back();
    out();
}

void Scope::in_switch(char* lbreak) {
    breaks.push_back(lbreak);
    defaults.push_back(nullptr);
    cases.push_back(std::vector<CaseTuple>());
    in();
}

void Scope::out_switch() {
    breaks.pop_back();
    defaults.pop_back();
    cases.pop_back();
    out();
}

Scope* Scope::copy() {
    return new Scope(this);
}

void Scope::clear_local() {
    assert(local_envs_backup.size() == 0);
    swap(local_envs, local_envs_backup);
}

void Scope::recover_local() {
    assert(local_envs.size() == 0);
    swap(local_envs, local_envs_backup);
}