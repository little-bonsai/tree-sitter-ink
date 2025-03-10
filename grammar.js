function interleave(exp, sep) {
  return seq(exp, repeat(seq(sep, exp)));
}

function interleave0(exp, sep) {
  return seq(optional(exp), repeat(seq(sep, exp)));
}

/** seq, where everything is optional, but at least one thing must be present */
function nonNullOptSeq(...opts) {
  const acc = [];
  for (let i = 0; i < opts.length; i++) {
    const acc_ = [];
    for (let j = 0; j < opts.length; j++) {
      if (i === j) {
        acc_.push(opts[j]);
      } else {
        acc_.push(optional(opts[j]));
      }
    }

    acc.push(seq(...acc_));
  }

  return choice(...acc);
}

const mixedTextDelimited = ($, delimiter = "") =>
  nonNullOptSeq(
    repeat1(
      choice(
        $.blockComment,
        $.glue,
        $.inlineSequence,
        $.inlineConditional,
        $.interpolation,
        alias($[`contentText${delimiter}`], $.contentText),
      ),
    ),
    $.divert,
    $.thread,
    $.tunnel,
  );

module.exports = grammar({
  name: "ink",
  extras: () => [" ", "\t", "\r"],
  word: ($) => $.identifier,

  externals: ($) => [
    $.contentText,
    $.contentTextQuote,
    $.contentTextPipe,

    $.choiceBullets,
    $.gatherBullets,

    $._choiceConditionOpen,

    $._inlineConditionalOpen,

    $._switchCaseOpen,

    $._inlineSequenceOpen,
    $._inlineSequenceSep,
  ],

  conflicts: ($) => [
    [$.knot],
    [$.stitch],
    [$._expression, $.path],
    [$.functionCall, $.path],
    [$.tunnel, $.divert],
    [$.path, $.identifierNamespaced],
    [$._expression, $.listSet],
    [$._expression, $.listSet, $.path],
  ],

  rules: {
    file: ($) => repeat($._topLevelStatement),

    _topLevelStatement: ($) =>
      choice($._knotBody, $.knot, $.include, $.variableDef, $.constantDef, $.listDef, $.externalFunction),
    _knotBody: ($) => choice($._stitchBody, $.stitch),
    _stitchBody: ($) =>
      prec(2, choice(prec(2, $.statement), prec(1, $._lineOfMixedText), $.choice, $.gather, $.switchCase)),

    knot: ($) => seq($.knotHeader, repeat1($._knotBody)),
    knotHeader: ($) =>
      seq(
        /===+/,
        optional("function"),
        field("name", $.identifier),
        optional(seq("(", optional(interleave(field("parameter", seq(optional("ref"), $.identifier)), ",")), ")")),
        /===+\n/,
      ),

    externalFunction: ($) =>
      seq(
        "EXTERNAL",
        field("name", $.identifier),
        optional(seq("(", optional(interleave(field("parameter", seq(optional("ref"), $.identifier)), ",")), ")")),
      ),

    stitch: ($) => seq($.stitchHeader, repeat1($._stitchBody)),
    stitchHeader: ($) => seq("=", $.identifier, "\n"),

    _lineOfMixedText: ($) => seq(optional(mixedTextDelimited($)), repeat($.tag), optional($._inlineComment), "\n"),

    _mixedText: ($) => mixedTextDelimited($),
    _mixedTextPipe: ($) => mixedTextDelimited($, "Pipe"),
    _mixedTextQuote: ($) => mixedTextDelimited($, "Quote"),

    interpolation: ($) => seq("{", $._expression, "}"),

    inlineConditional: ($) =>
      seq(
        $._inlineConditionalOpen,
        $._expression,
        ":",
        field("then", $._mixedTextPipe),
        optional(seq("|", field("else", $._mixedText))),
        "}",
      ),

    inlineSequence: ($) =>
      seq(
        $._inlineSequenceOpen,
        optional(field("kind", choice("&", "!", "~"))),
        interleave(optional($._mixedText), $._inlineSequenceSep),
        "}",
      ),

    choice: ($) =>
      prec(
        2,
        seq(
          $.choiceBullets,
          field("label", optional(seq("(", $.path, ")", /\s*/))),
          field("conditions", repeat(seq($._choiceConditionOpen, $._expression, "}", /\s*/))),
          optional("\n"),
          field("text", $._choiceText),
        ),
      ),
    _choiceText: ($) =>
      seq(
        optional(
          seq(optional(field("preSupression", $._mixedText)), "[", optional(field("inSupression", $._mixedText)), "]"),
        ),
        field("postSupression", $._lineOfMixedText),
      ),

    gather: ($) =>
      seq($.gatherBullets, optional(field("label", seq("(", $.path, ")", /\s*/))), field("text", $._lineOfMixedText)),

    switchCase: ($) =>
      seq(
        $._switchCaseOpen,
        repeat("\n"),
        field("over", optional($._switchCaseOver)),
        repeat(seq("-", field("condition", $._expression), ":", field("body", repeat1($._stitchBody)))),
        optional(seq("-", "else", ":", field("body", repeat1($._stitchBody)))),
        "}",
      ),
    _switchCaseOver: ($) => seq($._expression, ":", field("then", optional(repeat($._stitchBody)))),

    statement: ($) =>
      seq("~", choice($._expression, $._assignment, $.tempDef, $.return), optional($.lineComment), "\n"),
    _assignment: ($) => prec(2, seq($.identifier, choice("=", "+=", "-="), $._expression)),

    _expression: ($) =>
      choice(
        $.identifier,
        $.identifierNamespaced,
        $._exp_prefix,
        $._exp_postfix,
        $._exp_binary,
        $._exp_bracks,
        $.number,
        $.string,
        $.path,
        $.functionCall,
        $.divert,
        $.listSet,
      ),
    _exp_prefix: ($) => seq(choice("!", "not"), $._expression),
    _exp_postfix: ($) => prec.left(1, seq($._expression, choice("++", "--"))),
    _exp_bracks: ($) => prec.left(1, seq("(", $._expression, ")")),
    _exp_binary: ($) =>
      choice(
        ...["?", ">", "<", ">=", "<=", "==", "!=", "-", "+", "*", "/", "and", "&&", "or", "||"].map((op, idx) =>
          prec.left(idx + 1, seq($._expression, field("op", op), $._expression)),
        ),
      ),

    listDef: ($) =>
      seq(
        "LIST",
        seq(
          field("name", $.identifier),
          "=",
          interleave(
            field(
              "values",
              choice(
                field("initial", seq("(", $.identifier, optional(seq("=", $.number)), ")")),
                field("initial", seq("(", $.identifier, ")", optional(seq("=", $.number)))),
                seq($.identifier, optional(seq("=", $.number))),
              ),
            ),
            ",",
          ),
        ),
        "\n",
      ),

    listSet: ($) => seq("(", interleave0(choice($.identifier, $.identifierNamespaced), ","), ")"),
    functionCall: ($) => seq($.identifier, "(", optional(interleave($._expression, ",")), ")"),
    return: ($) => seq("return", $._expression),
    blockComment: () => seq("/*", /[^(\*\/)]*/, "*/"),
    divert: ($) => seq("->", optional(field("to", $.path))),
    thread: ($) => seq("<-", optional(field("from", $.path))),
    tunnel: ($) => seq("->", optional(field("to", $.path)), "->", optional(field("return", $.path))),
    path: ($) => seq(interleave($.identifier, "."), optional(seq("(", optional(interleave($._expression, ",")), ")"))),
    glue: () => "<>",
    include: ($) => seq("INCLUDE", $.includePath, "\n"),
    includePath: () => /.*[^\n]/,
    variableDef: ($) => seq("VAR", $._assignment, "\n"),
    constantDef: ($) => seq("CONST", $._assignment, "\n"),
    tempDef: ($) => seq("temp", $._assignment),
    _inlineComment: ($) => choice($.lineComment, $.todoComment),
    lineComment: () => /\/\/.*/,
    todoComment: () => /todo.*/i,
    tag: ($) => seq("#", $._mixedText),
    number: () => /\d+(\.\d+)?/,
    string: ($) => seq('"', optional($._mixedTextQuote), '"'),
    identifier: () => /\w[\w\d]*/,
    identifierNamespaced: ($) => seq(field("namespace", $.identifier), ".", $.identifier),
  },
});
