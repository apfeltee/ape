#!/bin/zsh
args=()
for arg in "$@"; do
  if [[ "$arg" == "-d" ]]; then
    withvalgrind=1
  else
    args+=("$arg")
  fi
done
if [[ "$withvalgrind" == 1 ]]; then
  exec valgrind node "etc/nodeshim.js" "${args[@]}"
else
  exec node "etc/nodeshim.js" "${args[@]}"
fi
