// Classic worker for UMD decoder dependencies; module loading uses import().
importScripts("../vendor/xzwasm.js", "../vendor/fzstd.js");
self.onmessage = async ({ data: { info, maxBytes } }) => {
  try {
    const { fetchNar } = await import("./store.js");
    const entries = await fetchNar(
      info,
      (bytes) => postMessage({ type: "progress", bytes }),
      (source, bytes) => postMessage({ type: "cache", source, bytes }),
      { maxBytes, maxUnpackedBytes: 300 * 1024 ** 2 },
    );
    const buffers = [
      ...new Set(entries.filter((x) => x.data).map((x) => x.data.buffer)),
    ];
    postMessage({ type: "done", entries }, buffers);
  } catch (error) {
    postMessage({ type: "error", message: error.message });
  }
};
