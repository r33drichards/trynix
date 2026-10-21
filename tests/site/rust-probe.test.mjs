import { test } from "node:test";
import assert from "node:assert/strict";
import {
  base64,
  commandFor,
  parseResult,
  runProbe,
  fixtures,
} from "../../tools/rust-probe.mjs";
test("source and stdin become data, not shell interpolation", () => {
  const source = '$(touch /oops)\n"; exit 3';
  const c = commandFor({ source, stdin: "é" });
  assert.ok(c.includes(base64(source)));
  assert.ok(!c.includes("touch /oops"));
});
test("reject traversal, reserved names and oversized total input", () => {
  for (const n of ["../x", "/x", "main.rs", "x';id"])
    assert.throws(() => commandFor({ source: "", files: { [n]: "x" } }));
  assert.throws(() => commandFor({ source: "a".repeat(1025) }));
  assert.throws(() =>
    commandFor({ source: "a".repeat(600), stdin: "b".repeat(600) }),
  );
});
test("decode separate channels and nonzero exit", () => {
  const r = parseResult({
    timedOut: false,
    output:
      "COMPILE:0\nCOMPILER_OUT:\nCOMPILER_ERR:\nEXIT:7\nSTDOUT:aGkK\nSTDERR:ZXJyCg==\n",
  });
  assert.equal(r.exitCode, 7);
  assert.equal(r.stdout, "hi\n");
  assert.equal(r.stderr, "err\n");
});
test("missing completion never passes", () =>
  assert.throws(() => parseResult({ timedOut: false, output: "hello" })));
test("timeout destroys VM instead of pretending guest stopped", async () => {
  let destroyed = 0;
  const r = await runProbe(
    { run: async () => ({ timedOut: true, output: "" }) },
    fixtures[0],
    { destroyVM: async () => destroyed++ },
  );
  assert.equal(destroyed, 1);
  assert.equal(r.completed, false);
});
test("destruction callback is mandatory", async () =>
  assert.rejects(() => runProbe({}, fixtures[0])));

test("driver failures still destroy the VM", async () => {
  let destroyed = 0;
  await assert.rejects(() =>
    runProbe(
      {
        run: async () => {
          throw new Error("transport");
        },
      },
      fixtures[0],
      { destroyVM: async () => destroyed++ },
    ),
  );
  assert.equal(destroyed, 1);
});
