(string) @string
(identifier) @identifier
(number) @constant.numeric
(functionCall) @function
(includePath) @string
(knotHeader) @namespace
(knotHeader (identifier)) @namespace
(stitchHeader) @namespace
(stitchHeader (identifier)) @namespace
(tag) @tag
(lineComment) @comment
(blockComment) @comment
(todoComment) @info

(choiceBullets) @keyword.control
(gatherBullets) @keyword.control

(gather label: _) @label
(choice label: _) @label
(identifierNamespaced namespace: _) @namespace

((identifier) @constant.builtin (#any-of? @constant.builtin "END" "DONE"))
((identifier) @constant.builtin.boolean (#any-of? @constant.builtin.boolean "false" "true"))

["(" ")" "{" "}"] @punctuation
["->" "->" "<-"] @keyword.control
["?" ">" "<" ">=" "<=" "==" "!=" "-" "+" "*" "/" "and" "&&" "or" "||" "!" "not" "=" "+=" "-=" "++" "--"] @operator
["CONST"  "VAR"  "temp"  "LIST"] @keyword.storage
["EXTERNAL" "function"] @keyword.function
["INCLUDE"] @keyword.include
["return"] @keyword.control.return
