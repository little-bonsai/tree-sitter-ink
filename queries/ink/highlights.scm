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

["return"] @keyword.control.return
["INCLUDE" "->" "->" "<-"] @keyword.include
["EXTERNAL" "function"] @keyword.function
["?" ">" "<" ">=" "<=" "==" "!=" "-" "+" "*" "/" "and" "&&" "or" "||" "!" "not" "=" "+=" "-=" "++" "--"] @operator
["(" ")" "{" "}"] @punctuation
["CONST"  "VAR"  "temp"  "LIST"] @keyword.storage
