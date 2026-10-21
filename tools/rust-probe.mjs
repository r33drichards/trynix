// Browser/Node-compatible fixture harness for the TryNix guest command driver.
// This does not implement VM isolation: caller MUST destroy the VM after timeout.
export const MAX_INPUT_BYTES = 1024;
export const MAX_OUTPUT_BYTES = 65536;
const encode = (value) => new TextEncoder().encode(value);
export function base64(value) {
  const bytes = typeof value === "string" ? encode(value) : value;
  if (!(bytes instanceof Uint8Array))
    throw new TypeError("Expected text or bytes");
  if (bytes.length > MAX_INPUT_BYTES)
    throw new RangeError("Input exceeds 1024 bytes");
  return btoa(String.fromCharCode(...bytes));
}
function quote(value) {
  return `'${value.replaceAll("'", "'\\''")}'`;
}
export function commandFor({ source, stdin = "", files = {} }) {
  const entries = Object.entries(files);
  let total = encode(source).length + encode(stdin).length;
  for (const [name, bytes] of entries) {
    if (
      !/^[a-zA-Z0-9][a-zA-Z0-9_.-]{0,63}$/.test(name) ||
      ["main.rs", "stdin", "program", "stdout", "stderr"].includes(name)
    ) {
      throw new Error("Invalid or reserved input filename");
    }
    total += typeof bytes === "string" ? encode(bytes).length : bytes.length;
  }
  if (total > MAX_INPUT_BYTES)
    throw new RangeError("Total input exceeds 1024 bytes");
  const put = (name, data) =>
    `printf '%s' '${base64(data)}' | base64 -d > ${quote(name)}`;
  return [
    "d=$(mktemp -d /tmp/paste-rust.XXXXXX) || exit 125",
    'cd "$d" || exit 125',
    put("main.rs", source),
    put("stdin", stdin),
    ...entries.map(([name, bytes]) => put(name, bytes)),
    // BusyBox ash ulimit -f is 512-byte blocks. This is per file, not total disk.
    "(ulimit -f 16384; rustc main.rs -o program >stdout 2>stderr)",
    "compile=$?",
    // Compiler stdout/stderr remain separately distinguishable from execution.
    "printf 'COMPILE:%s\\n' \"$compile\"",
    "printf 'COMPILER_OUT:'; head -c 65536 stdout | base64 | tr -d '\\n'; printf '\\n'",
    "printf 'COMPILER_ERR:'; head -c 65536 stderr | base64 | tr -d '\\n'; printf '\\n'",
    'if [ "$compile" -eq 0 ]; then (ulimit -f 128; ./program <stdin >stdout 2>stderr); result=$?; else result=$compile; fi',
    "printf 'EXIT:%s\\n' \"$result\"",
    "printf 'STDOUT:'; head -c 65536 stdout | base64 | tr -d '\\n'; printf '\\n'",
    "printf 'STDERR:'; head -c 65536 stderr | base64 | tr -d '\\n'; printf '\\n'",
    'cd /; rm -rf "$d"',
    'exit "$result"',
  ].join("; ");
}
export function parseResult(result) {
  if (result.timedOut)
    return { completed: false, reason: "timeout", output: result.output };
  const fields = Object.fromEntries(
    result.output
      .replaceAll("\r", "")
      .split("\n")
      .map((line) => {
        const at = line.indexOf(":");
        return [line.slice(0, at), line.slice(at + 1)];
      }),
  );
  if (!/^\d+$/.test(fields.COMPILE ?? "") || !/^\d+$/.test(fields.EXIT ?? "")) {
    throw new Error(
      "Missing compile/exit result; do not treat this as success",
    );
  }
  const decode = (key) => {
    if (!(key in fields)) throw new Error(`Missing ${key}`);
    const bytes = Uint8Array.from(atob(fields[key]), (c) => c.charCodeAt(0));
    if (bytes.length > MAX_OUTPUT_BYTES)
      throw new RangeError("Output exceeds bound");
    return new TextDecoder().decode(bytes);
  };
  return {
    completed: true,
    compileExit: Number(fields.COMPILE),
    exitCode: Number(fields.EXIT),
    compilerStdout: decode("COMPILER_OUT"),
    compilerStderr: decode("COMPILER_ERR"),
    stdout: decode("STDOUT"),
    stderr: decode("STDERR"),
  };
}
export const fixtures = [
  {
    name: "hello",
    source: 'fn main(){println!("hello");eprintln!("diagnostic");}',
    expect: { exitCode: 0, stdout: "hello\n", stderr: "diagnostic\n" },
  },
  {
    name: "stdin-file",
    source:
      'use std::io::{self,Read}; use std::fs; fn main(){let mut s=String::new();io::stdin().read_to_string(&mut s).unwrap();println!("{}:{}",s,fs::read_to_string("input.txt").unwrap());}',
    stdin: "stdin",
    files: { "input.txt": "file" },
    expect: { exitCode: 0, stdout: "stdin:file\n" },
  },
  {
    name: "nonzero",
    source: "fn main(){std::process::exit(7);}",
    expect: { exitCode: 7 },
  },
  {
    name: "syntax-error",
    source: "fn main( {",
    expect: { compileFailure: true },
  },
  { name: "infinite", source: "fn main(){loop {}}", expect: { timeout: true } },
];
export async function runProbe(
  driver,
  fixture,
  { timeoutMs = 120000, destroyVM } = {},
) {
  if (typeof destroyVM !== "function")
    throw new Error("VM destruction callback required");
  const started = performance.now();
  try {
    const result = await driver.run(commandFor(fixture), { timeoutMs });
    return { ...parseResult(result), elapsedMs: performance.now() - started };
  } finally {
    // Fresh guest state is mandatory even after parse errors or rejected calls.
    // This cannot interrupt a blocked UI thread: owner must enforce a worker deadline.
    await destroyVM();
  }
}
