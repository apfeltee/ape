#!/bin/zsh

# for some reason cproto keeps expanding 'bool' ...
cproto *.c | perl -pe 's/\b_Bool\b/bool/g'

