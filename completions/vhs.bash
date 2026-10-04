_vhs()
{
    local current previous
    current=${COMP_WORDS[COMP_CWORD]}
    previous=${COMP_WORDS[COMP_CWORD-1]}

    if [ "$COMP_CWORD" -eq 1 ]; then
        COMPREPLY=( $(compgen -W 'new check manual record version help' -- "$current") )
        COMPREPLY+=( $(compgen -f -X '!*.tape' -- "$current") )
        return
    fi
    case "$previous" in
        new|check) COMPREPLY=( $(compgen -f -- "$current") ) ;;
    esac
}
complete -F _vhs vhs
