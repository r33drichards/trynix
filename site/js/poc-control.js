// Opt-in Rust experiment controls. These are host budgets, not guest RAM limits.
export const POC_LIMITS = Object.freeze({
  download: 256 * 1024 ** 2,
  unpacked: 768 * 1024 ** 2,
  archive: 300 * 1024 ** 2,
  paths: 64,
});
export function checkClosureBudget(infos, limits = POC_LIMITS) {
  let download = 0,
    unpacked = 0;
  if (infos.length > limits.paths)
    throw new Error("Closure path budget exceeded");
  for (const info of infos) {
    if (
      !Number.isSafeInteger(info.fileSize) ||
      info.fileSize < 0 ||
      !Number.isSafeInteger(info.narSize) ||
      info.narSize < 0
    )
      throw new Error("Invalid closure size metadata");
    if (info.narSize > limits.archive)
      throw new Error("Single archive unpacked budget exceeded");
    download += info.fileSize;
    unpacked += info.narSize;
  }
  if (download > limits.download || unpacked > limits.unpacked)
    throw new Error(
      `Closure budget exceeded: ${download} download bytes, ${unpacked} unpacked bytes`,
    );
  return { download, unpacked, paths: infos.length };
}
// One archive per worker lifetime bounds retained decoder state. Abort terminates
// the worker even while a synchronous decoder is busy. Never pretend this kills
// the separate VM: the session owner must dispose both on failure/cancel.
export function loadNarInWorker(
  info,
  {
    signal,
    onBytes = () => {},
    onCacheRead = () => {},
    timeoutMs = 120000,
    createWorker = () =>
      new Worker(new URL("./nar-worker.js", import.meta.url)),
  } = {},
) {
  checkClosureBudget([info]);
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      reject(new Error("Cancelled"));
      return;
    }
    const worker = createWorker();
    let finished = false;
    const finish = (error, entries) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      signal?.removeEventListener("abort", abort);
      worker.terminate();
      if (error) reject(error);
      else resolve(entries);
    };
    const abort = () => finish(new Error("Cancelled"));
    const timer = setTimeout(
      () => finish(new Error("Archive worker deadline exceeded")),
      timeoutMs,
    );
    signal?.addEventListener("abort", abort, { once: true });
    worker.onerror = (event) =>
      finish(new Error(event.message || "Archive worker failed"));
    worker.onmessage = ({ data }) => {
      if (data.type === "progress") onBytes(data.bytes);
      else if (data.type === "cache") onCacheRead(data.source, data.bytes);
      else if (data.type === "done") finish(null, data.entries);
      else if (data.type === "error") finish(new Error(data.message));
    };
    worker.postMessage({ info, maxBytes: POC_LIMITS.download });
  });
}

// The owner event loop must remain responsive. This is not a host OS CPU quota.
export async function runGuestWithDeadline(
  vm,
  driver,
  command,
  timeoutMs = 120000,
) {
  let timer;
  try {
    return await Promise.race([
      driver.run(command, { timeoutMs }).then((result) => {
        if (result.timedOut) throw new Error("Guest deadline exceeded");
        return result;
      }),
      new Promise((_, reject) => {
        timer = setTimeout(
          () => reject(new Error("Guest deadline exceeded")),
          timeoutMs,
        );
      }),
    ]);
  } finally {
    clearTimeout(timer);
    vm.dispose(); // Fresh guest per run, including failure and missing completion.
  }
}
