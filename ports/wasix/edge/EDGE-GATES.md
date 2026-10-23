# Edge feasibility checkpoint, 2026-10-23

This is local preparatory work, not an Edge deployment. The local arbitrary-Lua HTTP prototype passes the three-guest execution/isolation/cancellation matrix and fake-child protocol regressions. User source is base64-transferred as data to an unprivileged guest process; see README.md for the limits and remaining security gates.

## Design and gates
- A small WASIX HTTP supervisor listens before launching the unchanged QEMU Wasm as a subprocess, using posix_spawn and stdin/stdout pipes. Tiny local spawn, bidirectional pipe and waitpid probe passed.
- GET /health responds before boot. POST /run accepts arbitrary Lua source and stdin; GET /jobs/{id} returns buffered output/status; POST /jobs/{id}/cancel kills and reaps the child. Requests are serialized with a fresh guest per request. This loopback-only development API has no authentication or caller ownership and must not be publicly exposed.
- Package the supervisor, QEMU, kernel/initramfs/BIOS and prepopulated mapped-file Lua closure. Approximately 61 MB raw payload before wrapper/packaging; no runtime downloads or persistent storage needed.
- Proposed Edge Proxy mode, single_concurrency, one region, no InstaBoot or persistent volumes. An app instance may be evicted, so guest state is disposable. No assumption of cross-request affinity between different instances.
- Local tests proved HTTP readiness before boot, actual user-supplied Lua execution, fresh-request state, error exit codes and child cancellation/reaping. Next Edge-only probes must validate deployed backend/version, exception refs, shared memory, WASIX execution contexts, proc_spawn and package filesystem semantics.
- Memory observed locally is about 730–841 MiB host RSS; QEMU module declares initial768 MiB/max2 GiB and guest512 MiB. These are not Edge quota guarantees. Per-instance memory ceilings and gateway startup/request deadlines were not found in official docs checked.
- Wasmer's March2025 WordPress announcement describes LLVM and Wasm exceptions on Edge. It does not establish the exact runtime version or compatibility with this module's try_table/throw_ref/cache semantics. InstaBoot must remain disabled: the bounded exception-ref cache prohibits store snapshot/restore/transfer.

## Access and cost
Deployment requires an authenticated account with sufficient applicable quotas and approved access. No CLI token or deployment is included in this prototype. Public deployment additionally requires authentication and caller/job ownership, abuse and spending controls, a deployable listener configuration, and explicit approval of the bounded hosted test.

Official pricing currently advertises Hobby free: 6 active CPU hours, 400 GB-hours provisioned memory, 1 GB package storage, 150 GB bandwidth. Pro is $10/month with metered overages; not approved. A small smoke test may fit Hobby, but actual account allowance and enforced spending bounds must be confirmed before deployment. Do not infer memory charging from measured RSS.

## Sources
- https://docs.wasmer.io/edge/learn/deployment-modes/
- https://docs.wasmer.io/edge/configuration/
- https://docs.wasmer.io/edge/learn/instaboot/
- https://docs.wasmer.io/edge/faq/
- https://docs.wasmer.io/edge/architecture/
- https://wasmer.io/posts/wordpress-is-now-ga
- https://wasmer.io/pricing
