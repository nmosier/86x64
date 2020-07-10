# complete -W "help noop modify translate tweak convert" macho-tool
_macho_tool_completions() {
    COMPREPLY=(${COMP_WORDS[@]})
}

complete -F _macho_tool_completions macho-tool
