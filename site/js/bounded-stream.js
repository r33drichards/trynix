// Preallocate once rather than retaining decompressed chunks plus a joined copy.
export async function readSizedStream(
  stream,
  size,
  maxBytes = 300 * 1024 ** 2,
) {
  if (!Number.isSafeInteger(size) || size < 0 || size > maxBytes)
    throw new RangeError("Unpacked archive budget exceeded");
  const output = new Uint8Array(size);
  const reader = stream.getReader();
  let offset = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      if (value.byteLength > size - offset)
        throw new RangeError("Archive exceeds signed unpacked size");
      output.set(value, offset);
      offset += value.byteLength;
    }
    if (offset !== size)
      throw new Error("Archive shorter than signed unpacked size");
    return output;
  } catch (error) {
    await reader.cancel().catch(() => {});
    throw error;
  } finally {
    reader.releaseLock();
  }
}
