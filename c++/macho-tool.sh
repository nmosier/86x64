# complete -W "help noop modify translate tweak convert" macho-tool

_macho_tool_completions_command() {
    COMMAND=$1
    ARG="${COMP_WORDS[${#COMP_WORDS[@]}-1]}"
    
    cmd_help=""
    cmd_noop="--help"
    cmd_modify="--help --insert --delete --start"
    cmd_translate="--help --offset"
    cmd_tweak="--help --flags"
    cmd_convert="--help --archive"

    if [[ "$ARG" = -* ]]; then
        lookup=cmd_$COMMAND
        options="${!lookup}"
        COMPREPLY=($(compgen -W "$options" -- "$ARG"))
        return
    fi

    files=($(compgen -f -- "$ARG"))
    case $COMMAND in
        help)
            COMPREPLY=()
            ;;
        noop|modify|translate|tweak|convert)
            COMPREPLY=("${files[@]}")
            ;;
    esac
}


_macho_tool_completions() {
    # find command
    COMMAND="${COMP_WORDS[1]}"

    # check if in command list
    CMDS="help noop modify translate tweak convert"
    for REF in $CMDS; do
        if [[ $REF = "$COMMAND" ]]; then
            _macho_tool_completions_command $COMMAND
            return
        fi
    done

    COMPREPLY=($(compgen -W "$CMDS" -- $COMMAND))
}

complete -F _macho_tool_completions macho-tool
