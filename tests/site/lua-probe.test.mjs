import { test } from "node:test";
import assert from "node:assert/strict";
import { luaProbeCommand } from "../../tools/lua-probe.mjs";
test("Lua fixture uses bounded files and separate result markers", () => {
  const c = luaProbeCommand();
  assert.ok(c.length < 2000);
  for (const s of [
    "ulimit -f 128",
    "MAIN_EXIT:",
    "SYNTAX_EXIT:",
    "NONZERO_EXIT:",
    "/tmp/lua-poc-marker",
  ])
    assert.ok(c.includes(s));
  const sources = [
    ...c.matchAll(/printf '%s' '([^']+)' \| base64 -d > (\S+)/g),
  ];
  assert.equal(sources.length, 5);
  assert.ok(atob(sources[0][1]).includes('io.read("*a")'));
  assert.equal(atob(sources[4][1]), "os.exit(7)");
});
