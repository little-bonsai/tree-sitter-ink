# tree-sitter-ink

Highlights [ink][ink] scripts, includes [helix][helix] queries

## Developing

### Werk

this repo uses [werk][werk] as it's built tool. You can download it using the following command:

```bash
cargo install --git https://github.com/simonask/werk --rev 06f4e127 werk-cli
```

### Testing

You can run the full test suite through werk: `werk test-suite`.

This repo pulls in public known-good ink scripts to test against, If you have a public ink script that you would like included in the test suite, please open a PR.

[werk]: https://github.com/simonask/werk
[helix]: https://helix-editor.com/
[ink]: https://github.com/inkle/ink/
