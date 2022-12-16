#!/bin/zsh

grep ape_allocator_alloc mem.log | cut -d: -f2- | sort -h

