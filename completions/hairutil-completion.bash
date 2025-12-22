# Bash completion for hairutil (subcommands only).

_hairutil_subcommands="autofix convert decompose filter findpenet getcurvature info resample smooth stats subsample transform tubify"

_hairutil()
{
    local cur
    cur="${COMP_WORDS[COMP_CWORD]}"

    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "${_hairutil_subcommands}" -- "${cur}") )
    fi
}

complete -o default -o bashdefault -F _hairutil hairutil
