complete -c vhs -f
complete -c vhs -s h -l help -d 'Show help'
complete -c vhs -s v -l version -d 'Show version'
complete -c vhs -n '__fish_use_subcommand' -a new -d 'Create a starter tape'
complete -c vhs -n '__fish_use_subcommand' -a check -d 'Validate tape syntax'
complete -c vhs -n '__fish_use_subcommand' -a manual -d 'Show the command summary'
complete -c vhs -n '__fish_use_subcommand' -a record -d 'Record an interactive shell'
complete -c vhs -n '__fish_use_subcommand' -a version -d 'Show the version'
complete -c vhs -n '__fish_use_subcommand' -a help -d 'Show help'
complete -c vhs -n 'not __fish_seen_subcommand_from new check manual record version help' \
    -a '(__fish_complete_suffix .tape)'
