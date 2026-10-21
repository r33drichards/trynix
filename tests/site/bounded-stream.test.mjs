import { test } from "node:test";
import assert from "node:assert/strict";
import { readSizedStream } from "../../site/js/bounded-stream.js";
const stream = (...chunks) =>
  new ReadableStream({
    start(c) {
      for (const a of chunks) c.enqueue(new Uint8Array(a));
      c.close();
    },
  });
test("joins into one exact-sized output", async () =>
  assert.deepEqual(
    await readSizedStream(stream([1, 2], [3]), 3),
    new Uint8Array([1, 2, 3]),
  ));
test("rejects budget before touching stream", async () => {
  await assert.rejects(readSizedStream(null, 301 * 1024 ** 2), /budget/);
});
test("rejects short output", async () => {
  await assert.rejects(readSizedStream(stream([1]), 2), /shorter/);
});
test("rejects overflow and cancels reader", async () => {
  let cancelled = false;
  const s = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array(4));
    },
    cancel() {
      cancelled = true;
    },
  });
  await assert.rejects(readSizedStream(s, 2), /exceeds/);
  assert.equal(cancelled, true);
});

import { writeEntriesCooperatively } from "../../site/js/store.js";
test("materialization yields between 64-entry batches and honors cancel", async () => {
  const controller = new AbortController();
  let writes = 0;
  let yields = 0;
  const FS = {
    writeFile() {
      writes++;
    },
  };
  const entries = Array.from({ length: 130 }, (_, i) => ({
    type: "regular",
    path: String(i),
    data: new Uint8Array(),
  }));
  await assert.rejects(
    writeEntriesCooperatively(
      FS,
      "/root",
      entries,
      controller.signal,
      async () => {
        yields++;
        controller.abort();
      },
    ),
    /Cancelled/,
  );
  assert.equal(writes, 64);
  assert.equal(yields, 1);
});
test("pre-cancelled materialization does not write", async () => {
  const controller = new AbortController();
  controller.abort();
  await assert.rejects(
    writeEntriesCooperatively(
      {},
      "/root",
      [{ type: "regular" }],
      controller.signal,
    ),
    /Cancelled/,
  );
});
