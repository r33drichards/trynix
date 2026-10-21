# Temporary static preview; QEMU runs in the visitor's browser, not this server.
FROM nixos/nix:2.24.11 AS build
WORKDIR /src
COPY . .
RUN nix --extra-experimental-features 'nix-command flakes' \
    --option sandbox false --option accept-flake-config true \
    build .#site -L --out-link /result \
 && mkdir /public && cp -rL /result/. /public/

FROM python:3.13-alpine
WORKDIR /app
COPY --from=build /public /app/public
COPY tools/preview-server.py /app/server.py
ENV PORT=8080
EXPOSE 8080
CMD ["python", "/app/server.py"]
