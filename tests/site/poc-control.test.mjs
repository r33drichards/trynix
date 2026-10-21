import { test } from "node:test";
import assert from "node:assert/strict";
import {
  checkClosureBudget,
  loadNarInWorker,
  POC_LIMITS,
} from "../../site/js/poc-control.js";
const info = { fileSize: 20, narSize: 40 };
function fake() {
  return {
    terminated: 0,
    postMessage() {},
    terminate() {
      this.terminated++;
    },
  };
}
test("budget rejects modern rustc before creating worker", () => {
  assert.throws(() =>
    checkClosureBudget([{ fileSize: 332069688, narSize: 1109942048 }]),
  );
  assert.deepEqual(checkClosureBudget([info]), {
    download: 20,
    unpacked: 40,
    paths: 1,
  });
  assert.throws(() => checkClosureBudget([{ fileSize: -1, narSize: 2 }]));
  assert.throws(() =>
    checkClosureBudget(Array(POC_LIMITS.paths + 1).fill(info)),
  );
});
test("success terminates worker and returns transferred entries", async () => {
  const worker = fake();
  const p = loadNarInWorker(info, { createWorker: () => worker });
  worker.onmessage({ data: { type: "done", entries: [{ path: "x" }] } });
  assert.deepEqual(await p, [{ path: "x" }]);
  assert.equal(worker.terminated, 1);
});
test("abort rejects and terminates worker exactly once", async () => {
  const worker = fake(),
    controller = new AbortController();
  const p = loadNarInWorker(info, {
    signal: controller.signal,
    createWorker: () => worker,
  });
  controller.abort();
  await assert.rejects(p, /Cancelled/);
  assert.equal(worker.terminated, 1);
  worker.onmessage({ data: { type: "done", entries: [] } });
  assert.equal(worker.terminated, 1);
});
test("already aborted does not start worker", async () => {
  const controller = new AbortController();
  controller.abort();
  let created = false;
  await assert.rejects(
    loadNarInWorker(info, {
      signal: controller.signal,
      createWorker: () => {
        created = true;
      },
    }),
    /Cancelled/,
  );
  assert.equal(created, false);
});
test("deadline stops a busy decoder independently of its response", async () => {
  const worker = fake();
  await assert.rejects(
    loadNarInWorker(info, { timeoutMs: 5, createWorker: () => worker }),
    /deadline/,
  );
  assert.equal(worker.terminated, 1);
});
test("worker failures terminate and reject", async () => {
  const worker = fake();
  const p = loadNarInWorker(info, { createWorker: () => worker });
  worker.onerror({ message: "decode error" });
  await assert.rejects(p, /decode error/);
  assert.equal(worker.terminated, 1);
});
test("guest deadline disposes VM even when driver never returns", async () => {
  const { runGuestWithDeadline } = await import("../../site/js/poc-control.js");
  let stops = 0;
  await assert.rejects(
    runGuestWithDeadline(
      {
        dispose() {
          stops++;
        },
      },
      {
        run() {
          return new Promise(() => {});
        },
      },
      "loop",
      5,
    ),
    /deadline/,
  );
  assert.equal(stops, 1);
});
test("completed guest run also disposes VM for fresh state", async () => {
  const { runGuestWithDeadline } = await import("../../site/js/poc-control.js");
  let stops = 0;
  const result = await runGuestWithDeadline(
    {
      dispose() {
        stops++;
      },
    },
    { run: async () => ({ timedOut: false, status: 7 }) },
    "exit 7",
  );
  assert.equal(result.status, 7);
  assert.equal(stops, 1);
});
