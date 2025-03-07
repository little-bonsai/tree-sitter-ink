#!/bin/bash

tree-sitter generate
tree-sitter build
hx -g fetch
hx -g build
mkdir -p ~/.config/helix/runtime/queries/ink
cp ./queries/ink/highlights.scm ~/.config/helix/runtime/queries/ink/

