// One bounded shell command exercises several source files in one disposable VM.
const put = (name, source) =>
  `printf '%s' '${btoa(source)}' | base64 -d > ${name}`;
export function luaProbeCommand() {
  return [
    "mkdir -p /tmp/lua-poc; cd /tmp/lua-poc; ulimit -f 128",
    put(
      "main.lua",
      'io.write("hello Lua\\n"); io.stderr:write("diagnostic\\n"); local f=assert(io.open("input.txt")); io.write(io.read("*a"),":",f:read("*a"),"\\n")',
    ),
    put("input.txt", "file"),
    put("stdin", "stdin"),
    'lua main.lua <stdin >out 2>err; printf "MAIN_EXIT:%s\\n" "$?"; printf "OUT:"; base64 out | tr -d "\\n"; printf "\\nERR:"; base64 err | tr -d "\\n"; printf "\\n"',
    put("bad.lua", "local = broken"),
    'lua bad.lua >badout 2>baderr; printf "SYNTAX_EXIT:%s\\n" "$?"; printf "SYNTAX_ERR:"; base64 baderr | tr -d "\\n"; printf "\\n"',
    put("exit.lua", "os.exit(7)"),
    'lua exit.lua; printf "NONZERO_EXIT:%s\\n" "$?"',
    "echo marker > /tmp/lua-poc-marker",
  ].join("; ");
}
