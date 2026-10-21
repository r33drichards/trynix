import { test } from "node:test";
import assert from "node:assert/strict";
import { fetchWithProgress } from "../../site/js/net.js";
let cached = null;
globalThis.caches = {
  open: async () => ({
    match: async () => cached,
    put: async () => {},
    delete: async () => {},
  }),
};
test("actual streamed bytes enforce cap without retrying oversized download", async () => {
  let fetches = 0;
  globalThis.fetch = async () => {
    fetches++;
    return new Response(new Uint8Array(9));
  };
  await assert.rejects(
    fetchWithProgress("https://example.invalid/test", { maxBytes: 8 }),
    /budget/,
  );
  assert.equal(fetches, 1);
});
test("cached response is also bounded", async () => {
  cached = new Response(new Uint8Array(9));
  await assert.rejects(
    fetchWithProgress("https://example.invalid/cached", { maxBytes: 8 }),
    /budget/,
  );
  cached = null;
});
test("within-budget response remains compatible", async () => {
  globalThis.fetch = async () => new Response(new Uint8Array([1, 2]));
  assert.deepEqual(
    await fetchWithProgress("https://example.invalid/ok", { maxBytes: 2 }),
    new Uint8Array([1, 2]),
  );
});
