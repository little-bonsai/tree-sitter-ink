#!/bin/bash

tree-sitter generate
tree-sitter build
cat test.ink
tree-sitter parse --timeout 1000 -0r  test.ink --debug 2> parse.log
